#include "terminal.h"

#include <signal.h>

#include "os.h"

#include "editor/editor.h"

#include "utils/utils.h"

void platformPanic(const char* file, int line, const char* s) {
    terminalExit();
#ifndef NDEBUG
    UNUSED(file);
    UNUSED(line);
    fprintf(stderr, "Fatal error: %s\r\n", s);
#else
    fprintf(stderr, "Fatal error at %s:%d: %s\r\n", file, line, s);
#endif
    exit(EXIT_FAILURE);
}

void platformMouseEnable(void) {
    writeConsoleStr(ANSI_MOUSE_ENABLE);
}

void platformMouseDisable(void) {
    writeConsoleStr(ANSI_MOUSE_DISABLE);
}

void platformCopyToSysClipboard(StrView sv) {
    int b64_len = base64EncodeLen(sv.size);
    char* b64_buf = malloc_s(b64_len * sizeof(char));
    b64_len = base64Encode(sv.data, sv.size, b64_buf);
    StrView b64 = {
        .size = b64_len - 1,  // exclude null terminator
        .data = b64_buf,
    };

    Str s = {0};

#ifndef _WIN32
    static bool tmux_check = false;
    static bool in_tmux;
    if (!tmux_check) {
        in_tmux = (getenv("TMUX") != NULL);
        tmux_check = true;
    }

    if (in_tmux) {
        strAppend(&s, svFromCStr("\x1bPtmux;\x1b"));
    }
#endif
    strAppend(&s, svFromCStr("\x1b]52;c;"));
    strAppend(&s, b64);
    strAppend(&s, svFromCStr("\x07"));

#ifndef _WIN32
    if (in_tmux) {
        strAppend(&s, svFromCStr("\x1b\\"));
    }
#endif

    writeConsoleAll(s.data, s.size);

    free(b64_buf);
    strFree(&s);
}

static void SIGSEGV_handler(int sig) {
    if (sig != SIGSEGV)
        return;
    terminalExit();
    writeConsoleStr("Segmentation fault\r\n");
    _exit(EXIT_FAILURE);
}

static void SIGABRT_handler(int sig) {
    if (sig != SIGABRT)
        return;
    terminalExit();
    writeConsoleStr("Aborted\r\n");
    _exit(EXIT_FAILURE);
}

static bool terminal_active = false;

void terminalStart(void) {
    terminal_active = true;
    enableRawMode();
    writeConsoleStr(
        ANSI_FOCUS_ENABLE ANSI_SWAP_ENABLE ANSI_BRACKETED_PASTE_ENABLE);

    if (signal(SIGSEGV, SIGSEGV_handler) == SIG_ERR) {
        PANIC("Failed to install SIGSEGV handler");
    }

    if (signal(SIGABRT, SIGABRT_handler) == SIG_ERR) {
        PANIC("Failed to install SIGABRT handler");
    }

    if (gEditor.mouse_mode) {
        platformMouseEnable();
    } else {
        platformMouseDisable();
    }

    int width, height;
    if (getWindowSize(&width, &height) == -1) {
        PANIC("Unable to query terminal window size");
    }

    uint64_t curr_time = getTimeMs();
    Event event = {
        .type = EVENT_RESIZE,
        .resize =
            {
                .width = width,
                .height = height,
            },
    };
    editorProcessEvent(event, curr_time);

    terminalRefreshScreen(true);
}

void terminalExit(void) {
    if (!terminal_active)
        return;
    terminal_active = false;
    writeConsoleStr(
        ANSI_MOUSE_DISABLE ANSI_BRACKETED_PASTE_DISABLE ANSI_SWAP_DISABLE
            ANSI_FOCUS_DISABLE ANSI_CLEAR_STYLE ANSI_CURSOR_SHOW);
    disableRawMode();
}
