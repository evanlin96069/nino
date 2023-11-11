#ifndef OS_UNIX_H
#define OS_UNIX_H

#define ENV_HOME "HOME"
#define CONF_DIR ".config/nino"
#define DIR_SEP "/"

#ifdef __linux__
// Linux
#include <linux/limits.h>
#define EDITOR_PATH_MAX PATH_MAX
#else
// Other
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

#endif
