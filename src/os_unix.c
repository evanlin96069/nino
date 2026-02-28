#include "os_unix.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>

#include "os.h"
#include "terminal.h"
#include "utils.h"

#define SIGWINCH_BYTE 0x01
#define SIGTSTP_BYTE 0x02
#define SIGCONT_BYTE 0x03

static int sig_rd = -1, sig_wr = -1;
static volatile sig_atomic_t winch_queued = 0;
static volatile sig_atomic_t tstp_queued = 0;

static int tty_fd = -1;

static void SIGWINCH_handler(int sig) {
    UNUSED(sig);
    if (!winch_queued) {
        winch_queued = 1;
        const uint8_t b = SIGWINCH_BYTE;
        UNUSED(write(sig_wr, &b, 1));
    }
}

static void SIGTSTP_handler(int sig) {
    UNUSED(sig);
    if (!tstp_queued) {
        tstp_queued = 1;
        const uint8_t b = SIGTSTP_BYTE;
        UNUSED(write(sig_wr, &b, 1));
    }
}

static void SIGCONT_handler(int sig) {
    UNUSED(sig);
    if (tstp_queued) {
        const uint8_t b = SIGCONT_BYTE;
        UNUSED(write(sig_wr, &b, 1));
    }
}

static int installSIGTSTPHandler(void) {
    struct sigaction tstp_action = {
        .sa_handler = SIGTSTP_handler,
    };
    sigemptyset(&tstp_action.sa_mask);
    return sigaction(SIGTSTP, &tstp_action, NULL);
}

void osInit(void) {
    tty_fd = open("/dev/tty", O_RDWR);
    if (tty_fd == -1)
        PANIC("Failed to open /dev/tty");

    int p[2];
    if (pipe(p) == -1) {
        PANIC("Failed to create pipe for signal handling");
    }
    sig_rd = p[0];
    sig_wr = p[1];

    struct sigaction winch_action = {
        .sa_handler = SIGWINCH_handler,
    };
    sigemptyset(&winch_action.sa_mask);
    if (sigaction(SIGWINCH, &winch_action, NULL) == -1) {
        PANIC("Failed to install SIGWINCH handler");
    }

    if (installSIGTSTPHandler() == -1) {
        PANIC("Failed to install SIGTSTP handler");
    }

    struct sigaction cont_action = {
        .sa_handler = SIGCONT_handler,
    };
    sigemptyset(&cont_action.sa_mask);
    if (sigaction(SIGCONT, &cont_action, NULL) == -1) {
        PANIC("Failed to install SIGCONT handler");
    }
}

static struct termios orig_termios;

bool isStdinTty(void) {
    return isatty(STDIN_FILENO);
}

void enableRawMode(void) {
    if (tcgetattr(tty_fd, &orig_termios) == -1)
        PANIC("Unable to read terminal attributes");

    struct termios raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(tty_fd, TCSAFLUSH, &raw) == -1)
        PANIC("Unable to enable raw terminal mode");
}

void disableRawMode(void) {
    UNUSED(tcsetattr(tty_fd, TCSAFLUSH, &orig_termios));
}

static bool readConsoleByte(uint8_t* out, int timeout_ms) {
    struct pollfd fds[2] = {
        {.fd = tty_fd, .events = POLLIN},
        {.fd = sig_rd, .events = POLLIN},
    };

    while (true) {
        int ret = poll(fds, 2, timeout_ms);
        if (ret <= 0)
            return false;

        if (fds[0].revents & POLLIN)
            return read(tty_fd, out, 1) == 1;

        if (fds[1].revents & POLLIN) {
            uint8_t buf[64];
            ssize_t n = read(sig_rd, buf, sizeof(buf));
            for (ssize_t i = 0; i < n; i++) {
                switch (buf[i]) {
                    case SIGWINCH_BYTE:
                        resizeWindow(false);
                        winch_queued = 0;
                        break;
                    case SIGTSTP_BYTE: {
                        struct sigaction sa = {
                            .sa_handler = SIG_DFL,
                        };
                        sigemptyset(&sa.sa_mask);
                        sigaction(SIGTSTP, &sa, NULL);

                        terminalExit();
                        raise(SIGTSTP);
                    } break;
                    case SIGCONT_BYTE:
                        installSIGTSTPHandler();
                        // Only restore if we're not in background
                        if (tcgetpgrp(tty_fd) == getpgrp()) {
                            terminalStart();
                            resizeWindow(true);
                            tstp_queued = 0;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

bool readConsole(uint32_t* unicode_out, int timeout_ms) {
    uint8_t first_byte;
    if (!readConsoleByte(&first_byte, timeout_ms)) {
        return false;
    }

    // ASCII fast-path
    if ((first_byte & 0x80) == 0x00) {
        *unicode_out = (uint32_t)first_byte;
        return true;
    }

    // Decode UTF-8
    int bytes;
    if ((first_byte & 0xE0) == 0xC0) {
        *unicode_out = (first_byte & 0x1F) << 6;
        bytes = 1;
    } else if ((first_byte & 0xF0) == 0xE0) {
        *unicode_out = (first_byte & 0x0F) << 12;
        bytes = 2;
    } else if ((first_byte & 0xF8) == 0xF0) {
        *unicode_out = (first_byte & 0x07) << 18;
        bytes = 3;
    } else {
        return false;
    }

    int shift = (bytes - 1) * 6;
    for (int i = 0; i < bytes; i++) {
        uint8_t byte;
        if (!readConsoleByte(&byte, READ_GRACE_MS))
            return false;
        if ((byte & 0xC0) != 0x80)
            return false;

        *unicode_out |= (byte & 0x3F) << shift;
        shift -= 6;
    }

    return true;
}

int writeConsole(const void* buf, size_t count) {
    return write(tty_fd, buf, count);
}

int getWindowSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(tty_fd, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    return -1;
}

FileInfo getFileInfo(const char* path) {
    FileInfo info;
    info.error = (stat(path, &info.info) == -1);
    return info;
}

bool areFilesEqual(FileInfo f1, FileInfo f2) {
    return (f1.info.st_ino == f2.info.st_ino &&
            f1.info.st_dev == f2.info.st_dev);
}

bool isFileModified(FileInfo f1, FileInfo f2) {
    return (f1.info.st_mtime != f2.info.st_mtime);
}

FileType getFileType(const char* path) {
    if (path[0] == '\0') {
        return FT_INVALID;
    }

    struct stat st;
    if (stat(path, &st) == -1) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return FT_NOT_EXIST;
        }
        if (errno == EACCES || errno == EPERM) {
            return FT_ACCESS_DENIED;
        }
        return FT_INVALID;
    }

    if (S_ISDIR(st.st_mode)) {
        return FT_DIR;
    }

    if (S_ISREG(st.st_mode)) {
        return FT_REG;
    }

    return FT_NOT_REG;
}

DirIter dirFindFirst(const char* path) {
    DirIter iter;
    iter.dp = opendir(path);
    if (iter.dp != NULL) {
        iter.entry = readdir(iter.dp);
        iter.error = (iter.entry == NULL);
    } else {
        iter.error = true;
    }
    return iter;
}

bool dirNext(DirIter* iter) {
    if (iter->error)
        return false;
    iter->entry = readdir(iter->dp);
    return iter->entry != NULL;
}

void dirClose(DirIter* iter) {
    if (iter->error)
        return;
    closedir(iter->dp);
}

const char* dirGetName(const DirIter* iter) {
    if (iter->error || !iter->entry)
        return NULL;
    return iter->entry->d_name;
}

bool pathExists(const char* path) {
    struct stat st;
    return (stat(path, &st) != -1);
}

bool canWriteFile(const char* path) {
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        // File doesn't exist yet, treat as writable
        return errno == ENOENT;
    }
    close(fd);
    return true;
}

FILE* openFile(const char* path, const char* mode) {
    return fopen(path, mode);
}

bool shouldSaveInPlace(const char* path) {
    struct stat st;
    if (lstat(path, &st) == -1) {
        return true;  // fail safe
    }

    // Symlink
    if (S_ISLNK(st.st_mode)) {
        return true;
    }

    // Hard-link
    return st.st_nlink > 1;
}

static OsError writeFile(int fd, const void* buf, size_t len) {
    OsError err;

    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, (char*)buf + off, len - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            err = errno;
            return err;
        }
        off += (size_t)w;
    }
    return 0;
}

OsError saveFileInPlace(const char* path, const void* buf, size_t len) {
    OsError err;

    int fd;
    struct stat st;
    mode_t mode = 0666;  // Default for new file

    if (stat(path, &st) == 0) {
        // File exists
        mode = st.st_mode & 0777;
    }

    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0) {
        err = errno;
        return err;
    }

    err = writeFile(fd, buf, len);
    if (err) {
        close(fd);
        return err;
    }

#ifndef NO_FSYNC
    if (fsync(fd) < 0) {
        int err = errno;
        close(fd);
        return err;
    }
#endif  // !NO_FSYNC

    close(fd);
    return 0;
}

OsError saveFileReplace(const char* path, const void* buf, size_t len) {
#ifdef NO_RENAME
    UNUSED(path);
    UNUSED(buf);
    UNUSED(len);
    return ENOSYS;  // Not supported
#else
    OsError err;

    char dir[EDITOR_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    getDirName(dir);

    char tmp_template[PATH_MAX];
    int tmp_len =
        snprintf(tmp_template, sizeof(tmp_template), "%s/.tmpXXXXXX", dir);
    if (tmp_len < 0 || tmp_len >= (int)sizeof(tmp_template)) {
        return ENAMETOOLONG;
    }

    int fd = mkstemp(tmp_template);
    if (fd < 0) {
        err = errno;
        return err;
    }

    // Set mode to match original
    struct stat st;
    if (stat(path, &st) == 0) {
        fchmod(fd, st.st_mode);
    }

    err = writeFile(fd, buf, len);
    if (err) {
        close(fd);
        unlink(tmp_template);
        return err;
    }

#ifndef NO_FSYNC
    if (fsync(fd) != 0) {
        err = errno;
        close(fd);
        unlink(tmp_template);
        return err;
    }
#endif  // !NO_FSYNC

    close(fd);

    if (rename(tmp_template, path) != 0) {
        err = errno;
        unlink(tmp_template);
        return err;
    }

#ifndef NO_FSYNC
    // fsync directory
    int dfd = open(dir, O_DIRECTORY | O_RDONLY);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }
#endif  // !NO_FSYNC

    return 0;
#endif  // NO_RENAME
}

bool changeDir(const char* path) {
    return chdir(path) == 0;
}

char* getFullPath(const char* path) {
    static char resolved_path[EDITOR_PATH_MAX];

    char parent_dir[EDITOR_PATH_MAX];
    char base_name[EDITOR_PATH_MAX];

    snprintf(parent_dir, sizeof(parent_dir), "%s", path);
    snprintf(base_name, sizeof(base_name), "%s", getBaseName(parent_dir));
    getDirName(parent_dir);
    if (parent_dir[0] == '\0') {
        parent_dir[0] = '.';
        parent_dir[1] = '\0';
    }

    char resolved_parent_dir[EDITOR_PATH_MAX];
    if (realpath(parent_dir, resolved_parent_dir) == NULL)
        return NULL;

    if (snprintf(resolved_path, sizeof(resolved_path), "%s/%s",
                 resolved_parent_dir, base_name) < 0)
        return NULL;

    return resolved_path;
}

void osSuspend(void) {
    kill(0, SIGTSTP);
}

int64_t getTime(void) {
    struct timeval time_val;
    gettimeofday(&time_val, NULL);
    return time_val.tv_sec * 1000000 + time_val.tv_usec;
}

void argsInit(int* argc, char*** argv) {
    UNUSED(argc);
    UNUSED(argv);
}

void argsFree(int argc, char** argv) {
    UNUSED(argc);
    UNUSED(argv);
}

void formatOsError(OsError err, char* buf, size_t len) {
    char* msg = strerror(err);
    snprintf(buf, len, "%s", msg);
}
