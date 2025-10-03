#include "os_win32.h"

#include <shellapi.h>

#include "os.h"
#include "terminal.h"

static HANDLE hStdin = INVALID_HANDLE_VALUE;
static HANDLE hStdout = INVALID_HANDLE_VALUE;

static UINT orig_cp_in;
static UINT orig_cp_out;
static DWORD orig_in_mode;
static DWORD orig_out_mode;

void osInit(void) {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        PANIC("GetStdHandle(STD_INPUT_HANDLE)");
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE)
        PANIC("GetStdHandle(STD_OUTPUT_HANDLE)");
}

void enableRawMode(void) {
    orig_cp_in = GetConsoleCP();
    orig_cp_out = GetConsoleOutputCP();

    if (!SetConsoleCP(CP_UTF8))
        PANIC("SetConsoleCP");

    if (!SetConsoleOutputCP(CP_UTF8))
        PANIC("SetConsoleOutputCP");

    DWORD mode = 0;

    if (!GetConsoleMode(hStdin, &mode))
        PANIC("GetConsoleMode(hStdin)");
    orig_in_mode = mode;
    mode |= ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS |
            ENABLE_VIRTUAL_TERMINAL_INPUT;
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
              ENABLE_QUICK_EDIT_MODE);
    if (!SetConsoleMode(hStdin, mode))
        PANIC("SetConsoleMode(hStdin)");

    if (!GetConsoleMode(hStdout, &mode))
        PANIC("GetConsoleMode(hStdout)");
    orig_out_mode = mode;
    mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
            DISABLE_NEWLINE_AUTO_RETURN;
    mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    if (!SetConsoleMode(hStdout, mode))
        PANIC("SetConsoleMode(hStdout)");
}

void disableRawMode(void) {
    SetConsoleMode(hStdin, orig_in_mode);
    SetConsoleMode(hStdout, orig_out_mode);
    SetConsoleCP(orig_cp_in);
    SetConsoleOutputCP(orig_cp_out);
}

static bool readConsoleWChar(WCHAR* out, int timeout_ms) {
    static DWORD repeat_left = 0;
    static WCHAR repeat_char = 0;

    if (repeat_left) {
        *out = repeat_char;
        repeat_left--;
        return true;
    }

    DWORD wait = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;

    if (wait != 0) {
        DWORD wr = WaitForSingleObject(hStdin, wait);
        if (wr == WAIT_TIMEOUT)
            return false;
        if (wr != WAIT_OBJECT_0)
            return false;
    }

    DWORD avail = 0;
    if (!GetNumberOfConsoleInputEvents(hStdin, &avail) || avail == 0)
        return false;

    COORD last_size = (COORD){0, 0};
    bool saw_resize = false;

    INPUT_RECORD rec;
    DWORD read = 0;

    while (avail--) {
        if (!ReadConsoleInputW(hStdin, &rec, 1, &read) || read == 0)
            break;

        switch (rec.EventType) {
            case WINDOW_BUFFER_SIZE_EVENT: {
                last_size = rec.Event.WindowBufferSizeEvent.dwSize;
                saw_resize = true;
            } break;

            case KEY_EVENT: {
                const KEY_EVENT_RECORD* ev = &rec.Event.KeyEvent;
                if (ev->bKeyDown && ev->uChar.UnicodeChar) {
                    const WCHAR ch = ev->uChar.UnicodeChar;
                    if (ev->wRepeatCount > 1) {
                        repeat_left = ev->wRepeatCount - 1;
                        repeat_char = ch;
                    }
                    *out = ch;
                    if (saw_resize)
                        setWindowSize(last_size.Y, last_size.X);
                    return true;
                }
            } break;

            default:
                break;
        }
    }

    if (saw_resize)
        setWindowSize(last_size.Y, last_size.X);
    return false;
}

static inline bool isHighSurrogate(WCHAR u) {
    return u >= 0xD800 && u <= 0xDBFF;
}

static inline bool isLowSurrogate(WCHAR u) {
    return u >= 0xDC00 && u <= 0xDFFF;
}

bool readConsole(uint32_t* unicode_out, int timeout_ms) {
    WCHAR b0;
    if (!readConsoleWChar(&b0, timeout_ms))
        return false;

    if (b0 < 0xD800 || b0 > 0xDFFF) {
        *unicode_out = b0;
        return true;
    }

    if (isHighSurrogate(b0)) {
        WCHAR b1;
        if (readConsoleWChar(&b1, READ_GRACE_MS) && isLowSurrogate(b1)) {
            uint32_t hs = (uint32_t)b0 - 0xD800;
            uint32_t ls = (uint32_t)b1 - 0xDC00;
            *unicode_out = 0x10000 + ((hs << 10) | ls);
            return true;
        }
    }

    // Invaild
    *unicode_out = 0xFFFD;
    return true;
}

int getWindowSize(int* rows, int* cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (GetConsoleScreenBufferInfo(hStdout, &csbi)) {
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return 0;
    }
    return getWindowSizeFallback(rows, cols);
}

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

const char* dirGetName(const DirIter* iter) {
    static char dir_name[EDITOR_PATH_MAX * 4];

    if (iter->error)
        return NULL;

    WideCharToMultiByte(CP_UTF8, 0, iter->find_data.cFileName, -1, dir_name,
                        EDITOR_PATH_MAX, NULL, false);
    return dir_name;
}

FILE* openFile(const char* path, const char* mode) {
    int size = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* w_path = malloc_s(size * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, size);

    size = MultiByteToWideChar(CP_UTF8, 0, mode, -1, NULL, 0);
    wchar_t* w_mode = malloc_s(size * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, w_mode, size);

    FILE* file = _wfopen(w_path, w_mode);

    free(w_path);
    free(w_mode);

    return file;
}

bool changeDir(const char* path) { return SetCurrentDirectory(path); }

char* getFullPath(const char* path) {
    static char resolved_path[EDITOR_PATH_MAX * 4];

    int size = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
    wchar_t* w_path = malloc_s(size * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, w_path, size);

    wchar_t w_resolved_path[EDITOR_PATH_MAX];
    GetFullPathNameW(w_path, EDITOR_PATH_MAX, w_resolved_path, NULL);

    WideCharToMultiByte(CP_UTF8, 0, w_resolved_path, -1, resolved_path,
                        EDITOR_PATH_MAX, NULL, false);

    free(w_path);

    return resolved_path;
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

void argsInit(int* argc, char*** argv) {
    LPWSTR* w_argv = CommandLineToArgvW(GetCommandLineW(), argc);
    if (!w_argv)
        PANIC("GetCommandLine");

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
