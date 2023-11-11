#include "os_unix.h"

#include <sys/time.h>

#include "os.h"
#include "utils.h"

FileInfo getFileInfo(const char* path) {
    FileInfo info;
    info.error = (stat(path, &info.info) == -1);
    return info;
}

bool areFilesEqual(FileInfo f1, FileInfo f2) {
    return f1.info.st_ino == f2.info.st_ino;
}

FileType getFileType(const char* path) {
    struct stat info;
    if (stat(path, &info) == -1)
        return FT_INVALID;
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

int64_t getTime(void) {
    struct timeval time_val;
    gettimeofday(&time_val, NULL);
    return time_val.tv_sec * 1000000 + time_val.tv_usec;
}

Args argsGet(int num_args, char** args) {
    return (Args){.count = num_args, .args = args};
}

void argsFree(Args args) { UNUSED(args.count); }
