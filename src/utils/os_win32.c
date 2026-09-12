#include "os_win32.h"

#include <shellapi.h>

#include "os.h"

#include "utils/utils.h"

FileInfo getFileInfo(const char* path) {
    FileInfo info;
    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    HANDLE hFile = CreateFileW(w_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        goto errdefer;

    BOOL result = GetFileInformationByHandle(hFile, &info.info);
    if (result == 0)
        goto errdefer;

    CloseHandle(hFile);
    info.error = false;
    return info;

errdefer:
    CloseHandle(hFile);
    info.error = true;
    return info;
}

bool areFilesEqual(FileInfo f1, FileInfo f2) {
    if (f1.error || f2.error)
        return false;
    return (f1.info.dwVolumeSerialNumber == f2.info.dwVolumeSerialNumber &&
            f1.info.nFileIndexHigh == f2.info.nFileIndexHigh &&
            f1.info.nFileIndexLow == f2.info.nFileIndexLow);
}

bool isFileModified(FileInfo f1, FileInfo f2) {
    if (f1.error || f2.error)
        return true;
    return (f1.info.ftLastWriteTime.dwLowDateTime !=
                f2.info.ftLastWriteTime.dwLowDateTime ||
            f1.info.ftLastWriteTime.dwHighDateTime !=
                f2.info.ftLastWriteTime.dwHighDateTime);
}

FileType getFileType(const char* path) {
    if (path[0] == '\0') {
        return FT_INVALID;
    }

    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    if (wcsncmp(w_path, L"\\\\.\\", 4) == 0) {
        return FT_INVALID;
    }

    DWORD attr = GetFileAttributesW(w_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return FT_NOT_EXIST;
        }
        if (err == ERROR_ACCESS_DENIED) {
            return FT_ACCESS_DENIED;
        }
        return FT_INVALID;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        return FT_DIR;
    }

    HANDLE h =
        CreateFileW(w_path, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_ACCESS_DENIED) {
            return FT_ACCESS_DENIED;
        }
        return FT_INVALID;
    }

    DWORD file_type = GetFileType(h);
    CloseHandle(h);

    if (file_type == FILE_TYPE_DISK) {
        return FT_REG;
    }

    return FT_NOT_REG;
}

DirIter dirFindFirst(const char* path) {
    DirIter iter;

    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    wchar_t entry_path[EDITOR_PATH_MAX];
    swprintf(entry_path, EDITOR_PATH_MAX, L"%ls\\*", w_path);

    iter.handle = FindFirstFileW(entry_path, &iter.find_data);
    iter.error = (iter.handle == INVALID_HANDLE_VALUE);

    return iter;
}

bool dirNext(DirIter* iter) {
    if (iter->error)
        return false;
    return FindNextFileW(iter->handle, &iter->find_data) != 0;
}

void dirClose(DirIter* iter) {
    if (iter->error)
        return;
    FindClose(iter->handle);
}

const char* dirGetName(const DirIter* iter) {
    static char dir_name[EDITOR_PATH_MAX * 4];

    if (iter->error)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, iter->find_data.cFileName, -1, dir_name,
                        EDITOR_PATH_MAX, NULL, false);
    return dir_name;
}

bool pathExists(const char* path) {
    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    DWORD attr = GetFileAttributesW(w_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return true;
}

bool canWriteFile(const char* path) {
    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    HANDLE h = CreateFileW(w_path, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        // File doesn't exist yet, treat as writable
        DWORD err = GetLastError();
        return err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND;
    }

    CloseHandle(h);
    return true;
}

FILE* openFile(const char* path, const char* mode) {
    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    wchar_t w_mode[32] = {0};
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, w_mode, 32);

    FILE* file = _wfopen(w_path, w_mode);
    return file;
}

static OsError writeFile(HANDLE h, const void* buf, size_t len) {
    OsError err;

    size_t off = 0;
    while (off < len) {
        size_t to_write = len - off;
        if (to_write > 0xFFFFFFFF) {
            to_write = 0xFFFFFFFF;
        }

        DWORD written;
        if (!WriteFile(h, (char*)buf + off, (DWORD)to_write, &written, NULL)) {
            err = GetLastError();
            return err;
        }

        off += (size_t)written;
    }

    return 0;
}

bool shouldSaveInPlace(const char* path) {
    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    // Symlink / junction
    DWORD attr = GetFileAttributesW(w_path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        return true;  // fail safe
    }

    if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
        return true;
    }

    // Hard-link
    HANDLE h =
        CreateFileW(w_path, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return true;  // fail safe
    }

    BY_HANDLE_FILE_INFORMATION info;
    bool result;
    if (GetFileInformationByHandle(h, &info)) {
        result = (info.nNumberOfLinks > 1);
    } else {
        result = true;  // fail safe
    }

    CloseHandle(h);
    return result;
}

OsError saveFileInPlace(const char* path, const void* buf, size_t len) {
    OsError err;

    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    DWORD attr = GetFileAttributesW(w_path);
    bool existed = (attr != INVALID_FILE_ATTRIBUTES);

    HANDLE h =
        CreateFileW(w_path, GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        err = GetLastError();
        return err;
    }

    // Truncate file
    if (!SetFilePointerEx(h, (LARGE_INTEGER){0}, NULL, FILE_BEGIN) ||
        !SetEndOfFile(h)) {
        err = GetLastError();
        CloseHandle(h);
        return err;
    }

    err = writeFile(h, buf, len);
    if (err) {
        CloseHandle(h);
        return err;
    }

    if (!FlushFileBuffers(h)) {
        err = GetLastError();
        CloseHandle(h);
        return err;
    }

    CloseHandle(h);

    // Restore original attributes if file existed
    if (existed) {
        SetFileAttributesW(w_path, attr);
    }

    return 0;
}

OsError saveFileReplace(const char* path, const void* buf, size_t len) {
    OsError err;

    char dir[EDITOR_PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    getDirName(dir);

    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    wchar_t w_dir[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, dir, -1, w_dir, EDITOR_PATH_MAX);

    wchar_t tmpname[EDITOR_PATH_MAX] = {0};
    if (!GetTempFileNameW(w_dir, L"tmp", 0, tmpname)) {
        err = GetLastError();
        return err;
    }

    HANDLE h = CreateFileW(tmpname, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        err = GetLastError();
        return err;
    }

    err = writeFile(h, buf, len);
    if (err) {
        CloseHandle(h);
        DeleteFileW(tmpname);
        return err;
    }

    if (!FlushFileBuffers(h)) {
        err = GetLastError();
        CloseHandle(h);
        DeleteFileW(tmpname);
        return err;
    }

    CloseHandle(h);
    h = INVALID_HANDLE_VALUE;

    DWORD attrs = GetFileAttributesW(w_path);
    bool target_exists = true;
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            target_exists = false;
        } else {
            DeleteFileW(tmpname);
            return err;
        }
    }

    if (target_exists) {
        if (!ReplaceFileW(w_path, tmpname, NULL,
                          REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
            err = GetLastError();
            DeleteFileW(tmpname);
            return err;
        }
    } else {
        if (!MoveFileExW(tmpname, w_path,
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            err = GetLastError();
            DeleteFileW(tmpname);
            return err;
        }
    }

    return 0;
}

bool changeDir(const char* path) {
    return SetCurrentDirectory(path);
}

char* getFullPath(const char* path) {
    static char resolved_path[(EDITOR_PATH_MAX) * 4];

    wchar_t w_path[EDITOR_PATH_MAX] = {0};
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);

    wchar_t w_resolved_path[EDITOR_PATH_MAX];
    GetFullPathNameW(w_path, EDITOR_PATH_MAX, w_resolved_path, NULL);

    WideCharToMultiByte(CP_UTF8, 0, w_resolved_path, -1, resolved_path,
                        EDITOR_PATH_MAX, NULL, false);

    return resolved_path;
}

int64_t getTimeMs(void) {
    return GetTickCount64();
}

void argsInit(int* argc, char*** argv) {
    LPWSTR* w_argv = CommandLineToArgvW(GetCommandLineW(), argc);
    if (!w_argv)
        PANIC("Failed to parse command line arguments");

    *argv = malloc_s(*argc * sizeof(char*));
    for (int i = 0; i < *argc; i++) {
        int size =
            WideCharToMultiByte(CP_UTF8, 0, w_argv[i], -1, NULL, 0, NULL, NULL);
        (*argv)[i] = malloc_s(size);
        WideCharToMultiByte(CP_UTF8, 0, w_argv[i], -1, (*argv)[i], size, NULL,
                            NULL);
    }
}

void argsFree(int argc, char** argv) {
    for (int i = 0; i < argc; i++) {
        free(argv[i]);
    }
    free(argv);
}

const char* getEnv(const char* name) {
    static char result[EDITOR_PATH_MAX * 4];

    wchar_t w_name[256] = {0};
    MultiByteToWideChar(CP_UTF8, 0, name, -1, w_name,
                        sizeof(w_name) / sizeof(wchar_t));

    wchar_t w_value[EDITOR_PATH_MAX] = {0};
    if (GetEnvironmentVariableW(w_name, w_value, EDITOR_PATH_MAX) == 0) {
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, w_value, -1, result, sizeof(result), NULL,
                        NULL);
    return result;
}

void formatOsError(OsError err, char* buf, size_t len) {
    DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

    DWORD n = FormatMessageA(flags, NULL, err,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                             (DWORD)len, NULL);

    if (n == 0) {
        snprintf(buf, len, "Windows error %lu", err);
    }
}
