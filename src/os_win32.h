#ifndef OS_WIN32_H
#define OS_WIN32_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#define ENV_HOME "USERPROFILE"
#define CONF_DIR "." EDITOR_NAME
#define DIR_SEP "\\"

#define EDITOR_PATH_MAX MAX_PATH

#include <io.h>

struct FileInfo {
    BY_HANDLE_FILE_INFORMATION info;

    bool error;
};

struct DirIter {
    HANDLE handle;
    WIN32_FIND_DATAW find_data;

    bool error;
};

typedef DWORD OsError;

#endif
