#include "os.h"

struct FileInfo {
    BY_HANDLE_FILE_INFORMATION info;

    bool error;
};

static inline FileInfo getFileInfo(const char* path) {
    FileInfo info;
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        goto defer;

    WINBOOL result = GetFileInformationByHandle(hFile, &info.info);
    if (result == 0)
        goto defer;

    CloseHandle(hFile);
    info.error = false;
    return info;

defer:
    CloseHandle(hFile);
    info.error = true;
    return info;
}

static inline bool areFilesEqual(FileInfo f1, FileInfo f2) {
    return (f1.info.dwVolumeSerialNumber == f2.info.dwVolumeSerialNumber &&
            f1.info.nFileIndexHigh == f2.info.nFileIndexHigh &&
            f1.info.nFileIndexLow == f2.info.nFileIndexLow);
}

static inline FileType getFileType(const char* path) {
    DWORD attri = GetFileAttributes(path);
    if (attri == INVALID_FILE_ATTRIBUTES)
        return FT_INVALID;
    if (attri & FILE_ATTRIBUTE_DIRECTORY)
        return FT_DIR;
    return FT_REG;
}

struct DirIter {
    HANDLE handle;
    WIN32_FIND_DATAA find_data;

    bool error;
};

static inline DirIter dirFindFirst(const char* path) {
    DirIter iter;
    char entry_path[EDITOR_PATH_MAX];
    snprintf(entry_path, sizeof(entry_path), "%s\\*", path);
    iter.handle = FindFirstFileA(entry_path, &iter.find_data);
    iter.error = (iter.handle == INVALID_HANDLE_VALUE);
    return iter;
}

static inline bool dirNext(DirIter* iter) {
    if (iter->error)
        return false;
    return FindNextFileA(iter->handle, &iter->find_data) != 0;
}

static inline void dirClose(DirIter* iter) {
    if (iter->error)
        return;
    FindClose(iter->handle);
}

static inline const char* dirGetName(DirIter iter) {
    if (iter.error)
        return NULL;
    return iter.find_data.cFileName;
}
