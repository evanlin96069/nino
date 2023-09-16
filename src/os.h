#ifndef OS_H
#define OS_H

#include <stdbool.h>


typedef struct FileInfo FileInfo;
static inline FileInfo getFileInfo(const char* path);
static inline bool areFilesEqual(FileInfo f1, FileInfo f2);

typedef enum FileType {
    FT_INVALID = -1,
    FT_REG,
    FT_DIR,
} FileType;
static inline FileType getFileType(const char* path);

typedef struct DirIter DirIter;
static inline DirIter dirFindFirst(const char* path);
static inline bool dirNext(DirIter* iter);
static inline void dirClose(DirIter* iter);
static inline const char* dirGetName(DirIter iter);

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>

#define ENV_HOME "USERPROFILE"
#define CONF_DIR ".nino"
#define DIR_SEP "\\"

#define EDITOR_PATH_MAX MAX_PATH

#include "os_win32.h"

#else

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

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

#include "os_unix.h"

#endif

#endif
