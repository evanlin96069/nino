#include "os.h"

#include "terminal.h"

#include "utils/os.h"

static HANDLE hStdin = INVALID_HANDLE_VALUE;
static HANDLE hStdout = INVALID_HANDLE_VALUE;
static HANDLE hConIn = INVALID_HANDLE_VALUE;
static HANDLE hConOut = INVALID_HANDLE_VALUE;

static UINT orig_cp_in;
static UINT orig_cp_out;
static DWORD orig_in_mode;
static DWORD orig_out_mode;

void terminalOsDeinit(void) {
    if (hConIn != INVALID_HANDLE_VALUE) {
        CloseHandle(hConIn);
        hConIn = INVALID_HANDLE_VALUE;
    }
    if (hConOut != INVALID_HANDLE_VALUE) {
        CloseHandle(hConOut);
        hConOut = INVALID_HANDLE_VALUE;
    }
}

void terminalOsInit(void) {
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        PANIC("Failed to get handle for standard input");
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE)
        PANIC("Failed to get handle for standard output");
    hConIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hConIn == INVALID_HANDLE_VALUE)
        PANIC("Failed to open console input");
    hConOut = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hConOut == INVALID_HANDLE_VALUE)
        PANIC("Failed to open console output");
}

bool isStdinTty(void) {
    return GetFileType(hStdin) == FILE_TYPE_CHAR;
}

void enableRawMode(void) {
    orig_cp_in = GetConsoleCP();
    orig_cp_out = GetConsoleOutputCP();

    if (!SetConsoleCP(CP_UTF8))
        PANIC("Failed to set UTF-8 input code page");

    if (!SetConsoleOutputCP(CP_UTF8))
        PANIC("Failed to set UTF-8 output code page");

    DWORD mode = 0;

    if (!GetConsoleMode(hConIn, &mode))
        PANIC("Failed to query console input mode");
    orig_in_mode = mode;
    mode |= ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS |
            ENABLE_VIRTUAL_TERMINAL_INPUT;
    mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
              ENABLE_QUICK_EDIT_MODE);
    if (!SetConsoleMode(hConIn, mode))
        PANIC("Failed to configure console input mode");

    if (!GetConsoleMode(hConOut, &mode))
        PANIC("Failed to query console output mode");
    orig_out_mode = mode;
    mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
            DISABLE_NEWLINE_AUTO_RETURN;
    mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    if (!SetConsoleMode(hConOut, mode))
        PANIC("Failed to configure console output mode");
}

void disableRawMode(void) {
    SetConsoleMode(hConIn, orig_in_mode);
    SetConsoleMode(hConOut, orig_out_mode);
    SetConsoleCP(orig_cp_in);
    SetConsoleOutputCP(orig_cp_out);
}

static bool has_pending_resize = false;
static ResizeEvent pending_resize = {0, 0};

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
        DWORD wr = WaitForSingleObject(hConIn, wait);
        if (wr == WAIT_TIMEOUT)
            return false;
        if (wr != WAIT_OBJECT_0)
            return false;
    }

    DWORD avail = 0;
    if (!GetNumberOfConsoleInputEvents(hConIn, &avail) || avail == 0)
        return false;

    COORD last_size = (COORD){0, 0};
    bool saw_resize = false;

    INPUT_RECORD rec;
    DWORD read = 0;

    while (avail--) {
        if (!ReadConsoleInputW(hConIn, &rec, 1, &read) || read == 0)
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
                    if (saw_resize) {
                        has_pending_resize = true;
                        pending_resize.width = last_size.X;
                        pending_resize.height = last_size.Y;
                    }
                    return true;
                }
            } break;

            default:
                break;
        }
    }

    if (saw_resize) {
        has_pending_resize = true;
        pending_resize.width = last_size.X;
        pending_resize.height = last_size.Y;
    }
    return false;
}

int writeConsole(const void* buf, size_t count) {
    DWORD bytes_written;
    if (WriteFile(hConOut, buf, count, &bytes_written, NULL)) {
        return (int)bytes_written;
    }
    return -1;
}

static inline bool isHighSurrogate(WCHAR u) {
    return u >= 0xD800 && u <= 0xDBFF;
}

static inline bool isLowSurrogate(WCHAR u) {
    return u >= 0xDC00 && u <= 0xDFFF;
}

ConsoleEvent readConsoleEvent(int timeout_ms) {
    ConsoleEvent ev = {.type = CONSOLE_EVENT_ERROR};

    if (has_pending_resize) {
        has_pending_resize = false;
        ev.type = CONSOLE_EVENT_RESIZE;
        ev.data.resize = pending_resize;
        return ev;
    }

    WCHAR b0;
    if (!readConsoleWChar(&b0, timeout_ms)) {
        if (has_pending_resize) {
            has_pending_resize = false;
            ev.type = CONSOLE_EVENT_RESIZE;
            ev.data.resize = pending_resize;
            return ev;
        }
        ev.type = CONSOLE_EVENT_TIMEOUT;
        return ev;
    }

    ev.type = CONSOLE_EVENT_KEY;

    if (b0 < 0xD800 || b0 > 0xDFFF) {
        ev.data.unicode = b0;
        return ev;
    }

    if (isHighSurrogate(b0)) {
        WCHAR b1;
        if (readConsoleWChar(&b1, READ_GRACE_MS) && isLowSurrogate(b1)) {
            uint32_t hs = (uint32_t)b0 - 0xD800;
            uint32_t ls = (uint32_t)b1 - 0xDC00;
            ev.data.unicode = 0x10000 + ((hs << 10) | ls);
            return ev;
        }
    }

    // Invalid
    ev.data.unicode = 0xFFFD;
    return ev;
}

int getWindowSize(int* rows, int* cols) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (GetConsoleScreenBufferInfo(hConOut, &csbi)) {
        *cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        *rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        return 0;
    }
    return -1;
}

void platformSuspend(void) {
    editorMsg("Suspend is unavailable on Windows.");
}

void platformRunShell(const char* shell_hint, const char* cmd) {
    wchar_t w_shell_buf[EDITOR_PATH_MAX] = {0};
    const wchar_t* shell = NULL;
    if (shell_hint && shell_hint[0]) {
        MultiByteToWideChar(CP_UTF8, 0, shell_hint, -1, w_shell_buf,
                            EDITOR_PATH_MAX);
        if (GetFileAttributesW(w_shell_buf) != INVALID_FILE_ATTRIBUTES) {
            shell = w_shell_buf;
        }
    }
    wchar_t w_comspec[EDITOR_PATH_MAX] = {0};
    if (!shell) {
        if (GetEnvironmentVariableW(L"COMSPEC", w_comspec, EDITOR_PATH_MAX) >
            0) {
            if (GetFileAttributesW(w_comspec) != INVALID_FILE_ATTRIBUTES) {
                shell = w_comspec;
            }
        }
    }
    if (!shell) {
        shell = L"cmd.exe";
    }

    wchar_t w_cmd[4096] = {0};
    MultiByteToWideChar(CP_UTF8, 0, cmd, -1, w_cmd,
                        sizeof(w_cmd) / sizeof(wchar_t));

    terminalExit();

    wchar_t full_cmd[EDITOR_PATH_MAX + 4096 + 8];
    swprintf(full_cmd, sizeof(full_cmd) / sizeof(wchar_t), L"\"%ls\" /C %ls",
             shell, w_cmd);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL ok = CreateProcessW(NULL, full_cmd, NULL, NULL, TRUE, 0, NULL, NULL,
                             &si, &pi);
    if (ok) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    const char* msg = "\r\nPress any key to continue...";
    DWORD written;
    WriteFile(hConOut, msg, (DWORD)strlen(msg), &written, NULL);

    // echo off
    DWORD old_in_mode;
    GetConsoleMode(hConIn, &old_in_mode);
    SetConsoleMode(hConIn,
                   old_in_mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    WCHAR ch;
    DWORD nread;
    ReadConsoleW(hConIn, &ch, 1, &nread, NULL);
    WriteFile(hConOut, "\r\n", 2, &written, NULL);

    SetConsoleMode(hConIn, old_in_mode);

    terminalStart();
}
