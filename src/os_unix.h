#ifndef OS_UNIX_H
#define OS_UNIX_H

#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

#define ENV_HOME "HOME"
#define CONF_DIR ".config/" EDITOR_NAME
#define DIR_SEP "/"

#ifdef PATH_MAX
#define EDITOR_PATH_MAX PATH_MAX
#else
#define EDITOR_PATH_MAX 4096
#endif

struct FileInfo {
    struct stat info;

    bool error;
};

struct DirIter {
    DIR* dp;
    struct dirent* entry;

    bool error;
};

typedef int OsError;

#endif
