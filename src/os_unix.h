#include "os.h"

struct FileInfo {
    struct stat info;

    bool error;
};

static inline FileInfo getFileInfo(const char* path) {
    FileInfo info;
    info.error = (stat(path, &info.info) == -1);
    return info;
}

static inline bool areFilesEqual(FileInfo f1, FileInfo f2) {
    return f1.info.st_ino == f2.info.st_ino;
}

static inline FileType getFileType(const char* path) {
    struct stat info;
    if (stat(path, &info) == -1)
        return FT_INVALID;
    if (S_ISDIR(info.st_mode))
        return FT_DIR;
    if (S_ISREG(info.st_mode))
        return FT_REG;
    return FT_INVALID;
}

struct DirIter {
    DIR* dp;
    struct dirent* entry;

    bool error;
};

static inline DirIter dirFindFirst(const char* path) {
    DirIter iter;
    iter.dp = opendir(path);
    if (iter.dp != NULL) {
        iter.entry = readdir(iter.dp);
        iter.error = (iter.entry == NULL);
        ;
    } else {
        iter.error = true;
    }
    return iter;
}

static inline bool dirNext(DirIter* iter) {
    if (iter->error)
        return false;
    iter->entry = readdir(iter->dp);
    return iter->entry != NULL;
}

static inline void dirClose(DirIter* iter) {
    if (iter->error)
        return;
    closedir(iter->dp);
}

static inline const char* dirGetName(const DirIter* iter) {
    if (iter->error || !iter->entry)
        return NULL;
    return iter->entry->d_name;
}
