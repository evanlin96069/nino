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

static int sig_rd = -1, sig_wr = -1;
static volatile sig_atomic_t winch_queued = 0;

static void SIGWINCH_handler(int sig) {
    if (sig != SIGWINCH)
        return;
    if (!winch_queued) {
        winch_queued = 1;
        const uint8_t b = 0x01;
        UNUSED(write(sig_wr, &b, 1));
    }
}

void osInit(void) {
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
}

static struct termios orig_termios;

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        PANIC("Unable to read terminal attributes");

    struct termios raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        PANIC("Unable to enable raw terminal mode");
}

void disableRawMode(void) {
    UNUSED(tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios));
}

static bool readConsoleByte(uint8_t* out, int timeout_ms) {
    struct pollfd fds[2] = {
        {.fd = STDIN_FILENO, .events = POLLIN},
        {.fd = sig_rd, .events = POLLIN},
    };

    while (true) {
        int ret = poll(fds, 2, timeout_ms);
        if (ret < 0)
            return false;

        if (fds[0].revents & POLLIN)
            return read(STDIN_FILENO, out, 1) == 1;

        if (fds[1].revents & POLLIN) {
            uint8_t buf[64];
            UNUSED(read(sig_rd, buf, sizeof(buf)));
            resizeWindow();
            winch_queued = 0;
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
    return write(STDOUT_FILENO, buf, count);
}

int getWindowSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0) {
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

FileType getFileType(const char* path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return FT_NOT_EXIST;
        }
        return FT_INVALID;
    }

    if (S_ISDIR(st.st_mode)) {
        return FT_DIR;
    }

    if (S_ISREG(st.st_mode)) {
        return FT_REG;
    }
    return FT_INVALID;
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

FILE* openFile(const char* path, const char* mode) { return fopen(path, mode); }

OsError saveFile(const char* path, const void* buf, size_t len) {
    OsError err;

    char dir[EDITOR_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    getDirName(dir);

    char tmp_template[PATH_MAX];
    int tmp_len =
        snprintf(tmp_template, sizeof(tmp_template), "%s/.tmpXXXXXX", dir);
    UNUSED(tmp_len);

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

    size_t off = 0;
    while (off < len) {
        ssize_t w = write(fd, (char*)buf + off, len - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            err = errno;
            close(fd);
            unlink(tmp_template);
            return err;
        }
        off += (size_t)w;
    }

    if (fsync(fd) != 0) {
        err = errno;
        close(fd);
        unlink(tmp_template);
        return err;
    }
    close(fd);

    if (rename(tmp_template, path) != 0) {
        err = errno;
        unlink(tmp_template);
        return err;
    }

    // fsync directory
    int dfd = open(dir, O_DIRECTORY | O_RDONLY);
    if (dfd >= 0) {
        fsync(dfd);
        close(dfd);
    }

    return OS_ERROR_SUCCESS;
}

bool changeDir(const char* path) { return chdir(path) == 0; }

char* getFullPath(const char* path) {
    static char resolved_path[EDITOR_PATH_MAX];
    if (realpath(path, resolved_path) == NULL) {
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

        int len = snprintf(resolved_path, sizeof(resolved_path), "%s/%s",
                           resolved_parent_dir, base_name);
        // This is just to suppress Wformat-truncation
        if (len < 0)
            return NULL;
    }
    return resolved_path;
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
