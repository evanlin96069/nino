#include "os_win32.h"

#include <shellapi.h>
#include <stdlib.h>

#include "os.h"
#include "utils.h"

FileInfo getFileInfo(const char* path) {
    FileInfo info;
    wchar_t w_path[EDITOR_PATH_MAX + 1] = {0};
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
    return (f1.info.dwVolumeSerialNumber == f2.info.dwVolumeSerialNumber &&
            f1.info.nFileIndexHigh == f2.info.nFileIndexHigh &&
            f1.info.nFileIndexLow == f2.info.nFileIndexLow);
}

FileType getFileType(const char* path) {
    DWORD attri = GetFileAttributes(path);
    if (attri == INVALID_FILE_ATTRIBUTES)
        return FT_INVALID;
    if (attri & FILE_ATTRIBUTE_DIRECTORY)
        return FT_DIR;
    return FT_REG;
}

DirIter dirFindFirst(const char* path) {
    DirIter iter;

    wchar_t w_path[EDITOR_PATH_MAX + 1] = {0};
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

static char dir_name[EDITOR_PATH_MAX * 4];
const char* dirGetName(const DirIter* iter) {
    if (iter->error)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, iter->find_data.cFileName, -1, dir_name,
                        EDITOR_PATH_MAX, NULL, false);
    return dir_name;
}

FILE* openFile(const char* path, const char* mode) {
    wchar_t w_path[EDITOR_PATH_MAX + 1] = {0};
    wchar_t short_path[EDITOR_PATH_MAX + 1];
    char c_path[EDITOR_PATH_MAX * 4];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, EDITOR_PATH_MAX);
    GetShortPathNameW(w_path, short_path, EDITOR_PATH_MAX);
    WideCharToMultiByte(CP_UTF8, 0, short_path, -1, c_path, EDITOR_PATH_MAX,
                        NULL, false);
    return fopen(c_path, mode);
}

int64_t getTime(void) {
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

    SYSTEMTIME system_time;
    FILETIME file_time;
    uint64_t time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;
    int64_t sec = ((time - EPOCH) / 10000000);
    int64_t usec = (system_time.wMilliseconds * 1000);
    return sec * 1000000 + usec;
}

Args argsGet(int num_args, char** args) {
    UNUSED(num_args);
    UNUSED(args);

    int argc;
    LPWSTR* w_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!w_argv)
        PANIC("GetCommandLine");

    char** utf8_argv = malloc_s(argc * sizeof(char*));
    for (int i = 0; i < argc; i++) {
        int size =
            WideCharToMultiByte(CP_UTF8, 0, w_argv[i], -1, NULL, 0, NULL, NULL);
        utf8_argv[i] = malloc_s(size);
        WideCharToMultiByte(CP_UTF8, 0, w_argv[i], -1, utf8_argv[i], size, NULL,
                            NULL);
    }

    return (Args){.count = argc, .args = utf8_argv};
}

void argsFree(Args args) {
    for (int i = 0; i < args.count; i++) {
        free(args.args[i]);
    }
    free(args.args);
}
