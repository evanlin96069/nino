#include "os_unix.h"

#include <errno.h>
#include <fcntl.h>
#include <time.h>

#include "utils/utils.h"

#include "os.h"

FileInfo getFileInfo(const char* path) {
    FileInfo info;
    info.error = (stat(path, &info.info) == -1);
    return info;
}

bool areFilesEqual(FileInfo f1, FileInfo f2) {
    if (f1.error || f2.error)
        return false;
    return (f1.info.st_ino == f2.info.st_ino &&
            f1.info.st_dev == f2.info.st_dev);
}

bool isFileModified(FileInfo f1, FileInfo f2) {
    if (f1.error || f2.error)
        return true;
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
        err = errno;
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

const char* getEnv(const char* name) {
    return getenv(name);
}

int64_t getTimeMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
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
