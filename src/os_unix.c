#define _GNU_SOURCE  // realpath

#include "os_unix.h"

#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>

#include "os.h"
#include "terminal.h"
#include "utils.h"

static struct termios orig_termios;

void osInit(void) {}

void enableRawMode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
        PANIC("tcgetattr");

    struct termios raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        PANIC("tcsetattr");
}

void disableRawMode(void) {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        PANIC("tcsetattr");
}

bool readConsole(uint32_t* unicode_out) {
    // Decode UTF-8

    int bytes;
    uint8_t first_byte;

    if (read(STDIN_FILENO, &first_byte, 1) != 1) {
        return false;
    }

    if ((first_byte & 0x80) == 0x00) {
        *unicode_out = (uint32_t)first_byte;
        return true;
    }

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

    uint8_t buf[3];
    if (read(STDIN_FILENO, buf, bytes) != bytes) {
        return false;
    }

    int shift = (bytes - 1) * 6;
    for (int i = 0; i < bytes; i++) {
        if ((buf[i] & 0xC0) != 0x80) {
            return false;
        }
        *unicode_out |= (buf[i] & 0x3F) << shift;
        shift -= 6;
    }

    return true;
}

int getWindowSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 || ws.ws_col != 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
    return getWindowSizeFallback(rows, cols);
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
    struct stat info;
    if (stat(path, &info) == -1)
        return FT_INVALID;
    if (S_ISCHR(info.st_mode))
        return FT_DEV;
    if (S_ISDIR(info.st_mode))
        return FT_DIR;
    if (S_ISREG(info.st_mode))
        return FT_REG;
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
