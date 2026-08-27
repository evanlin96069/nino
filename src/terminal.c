#include "terminal.h"

#include <signal.h>

#include "config.h"
#include "editor.h"
#include "os.h"
#include "unicode.h"
#include "utils.h"

typedef struct {
    const char* str;
    int value;
} StrIntPair;

static const StrIntPair sequence_lookup[] = {
    {"[1~", KEY_EVENT(KEY_HOME)},
    {"[2~", KEY_EVENT(KEY_INSERT)},
    {"[3~", KEY_EVENT(KEY_DELETE)},
    {"[4~", KEY_EVENT(KEY_END)},
    {"[5~", KEY_EVENT(KEY_PAGE_UP)},
    {"[6~", KEY_EVENT(KEY_PAGE_DOWN)},
    {"[7~", KEY_EVENT(KEY_HOME)},
    {"[8~", KEY_EVENT(KEY_END)},

    {"[A", KEY_EVENT(KEY_UP)},
    {"[B", KEY_EVENT(KEY_DOWN)},
    {"[C", KEY_EVENT(KEY_RIGHT)},
    {"[D", KEY_EVENT(KEY_LEFT)},
    {"[F", KEY_EVENT(KEY_END)},
    {"[H", KEY_EVENT(KEY_HOME)},

    /*
      Code     Modifiers
    ---------+---------------------------
       2     | Shift
       3     | Alt
       4     | Shift + Alt
       5     | Control
       6     | Shift + Control
       7     | Alt + Control
       8     | Shift + Alt + Control
       9     | Meta
       10    | Meta + Shift
       11    | Meta + Alt
       12    | Meta + Alt + Shift
       13    | Meta + Ctrl
       14    | Meta + Ctrl + Shift
       15    | Meta + Ctrl + Alt
       16    | Meta + Ctrl + Alt + Shift
    ---------+---------------------------
    */

    // Shift
    {"[1;2A", KEY_EVENT(KEY_MOD_SHIFT, KEY_UP)},
    {"[1;2B", KEY_EVENT(KEY_MOD_SHIFT, KEY_DOWN)},
    {"[1;2C", KEY_EVENT(KEY_MOD_SHIFT, KEY_RIGHT)},
    {"[1;2D", KEY_EVENT(KEY_MOD_SHIFT, KEY_LEFT)},
    {"[1;2F", KEY_EVENT(KEY_MOD_SHIFT, KEY_END)},
    {"[1;2H", KEY_EVENT(KEY_MOD_SHIFT, KEY_HOME)},

    // Alt
    {"[1;3A", KEY_EVENT(KEY_MOD_ALT, KEY_UP)},
    {"[1;3B", KEY_EVENT(KEY_MOD_ALT, KEY_DOWN)},
    {"[1;3C", KEY_EVENT(KEY_MOD_ALT, KEY_RIGHT)},
    {"[1;3D", KEY_EVENT(KEY_MOD_ALT, KEY_LEFT)},
    {"[1;3F", KEY_EVENT(KEY_MOD_ALT, KEY_END)},
    {"[1;3H", KEY_EVENT(KEY_MOD_ALT, KEY_HOME)},

    // Shift+Alt
    {"[1;4A", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_ALT, KEY_UP)},
    {"[1;4B", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_ALT, KEY_DOWN)},
    {"[1;4C", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_ALT, KEY_RIGHT)},
    {"[1;4D", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_ALT, KEY_LEFT)},
    {"[1;4F", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_ALT, KEY_END)},
    {"[1;4H", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_ALT, KEY_HOME)},

    // Ctrl
    {"[1;5A", KEY_EVENT(KEY_MOD_CTRL, KEY_UP)},
    {"[1;5B", KEY_EVENT(KEY_MOD_CTRL, KEY_DOWN)},
    {"[1;5C", KEY_EVENT(KEY_MOD_CTRL, KEY_RIGHT)},
    {"[1;5D", KEY_EVENT(KEY_MOD_CTRL, KEY_LEFT)},
    {"[1;5F", KEY_EVENT(KEY_MOD_CTRL, KEY_END)},
    {"[1;5H", KEY_EVENT(KEY_MOD_CTRL, KEY_HOME)},

    // Shift+Ctrl
    {"[1;6A", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_UP)},
    {"[1;6B", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_DOWN)},
    {"[1;6C", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_RIGHT)},
    {"[1;6D", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_LEFT)},
    {"[1;6F", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_END)},
    {"[1;6H", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_HOME)},

    // Alt+Ctrl
    {"[1;7A", KEY_EVENT(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_UP)},
    {"[1;7B", KEY_EVENT(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_DOWN)},
    {"[1;7C", KEY_EVENT(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_RIGHT)},
    {"[1;7D", KEY_EVENT(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_LEFT)},
    {"[1;7F", KEY_EVENT(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_END)},
    {"[1;7H", KEY_EVENT(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_HOME)},

    // Page UP / Page Down
    {"[5;2~", KEY_EVENT(KEY_MOD_SHIFT, KEY_PAGE_UP)},
    {"[6;2~", KEY_EVENT(KEY_MOD_SHIFT, KEY_PAGE_DOWN)},
    {"[5;5~", KEY_EVENT(KEY_MOD_CTRL, KEY_PAGE_UP)},
    {"[6;5~", KEY_EVENT(KEY_MOD_CTRL, KEY_PAGE_DOWN)},
    {"[5;6~", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_PAGE_UP)},
    {"[6;6~", KEY_EVENT(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_PAGE_DOWN)},
};

static bool parseMouseSGR(const char* seq,
                          int* Cb,
                          int* Cx,
                          int* Cy,
                          char* fin) {
    if (*seq != '<')
        return false;
    seq++;

    // Cb
    *Cb = atoi(seq);
    while (*seq && *seq != ';')
        seq++;
    if (*seq++ != ';')
        return false;

    // Cx
    *Cx = atoi(seq);
    while (*seq && *seq != ';')
        seq++;
    if (*seq++ != ';')
        return false;

    // Cy
    *Cy = atoi(seq);
    while (*seq && isDigit(*seq))
        seq++;

    if (*seq != 'M' && *seq != 'm')
        return false;
    *fin = *seq;

    return true;
}

static bool has_pending_resize = false;
static ConsoleResizeEvent pending_resize = {0, 0};

// Reads a raw key. Skips resize events.
static ConsoleEventType readConsoleKey(uint32_t* out, int timeout_ms) {
    while (true) {
        ConsoleEvent ev = readConsoleEvent(timeout_ms);
        switch (ev.type) {
            case CONSOLE_EVENT_KEY:
                *out = ev.data.unicode;
                return CONSOLE_EVENT_KEY;

            case CONSOLE_EVENT_RESIZE:
                has_pending_resize = true;
                pending_resize = ev.data.resize;
                break;

            default:
                return ev.type;
        }
    }
}

// ANSII escape sequences parsing.
Event eventPoll(int timeout_ms) {
    Event result = {.type = EVENT_ERROR};
    ConsoleEvent ev;
    uint32_t c;

    if (has_pending_resize) {
        has_pending_resize = false;
        ev.type = CONSOLE_EVENT_RESIZE;
        ev.data.resize = pending_resize;
    } else {
        ev = readConsoleEvent(timeout_ms);
    }

    switch (ev.type) {
        case CONSOLE_EVENT_ERROR:
            result.type = EVENT_ERROR;
            return result;

        case CONSOLE_EVENT_TIMEOUT:
            result.type = EVENT_TIMEOUT;
            return result;

        case CONSOLE_EVENT_KEY:
            c = ev.data.unicode;
            break;

        case CONSOLE_EVENT_RESIZE:
            result.type = EVENT_RESIZE;
            result.resize = ev.data.resize;
            return result;

        default:
            result.type = EVENT_ERROR;
            return result;
    }

    int timeout = ttimeoutlen.int_value;

    // CONSOLE_EVENT_KEY
    if (c == '\x1b') {  // ESC
        result.type = EVENT_KEY;
        result.key.value = KEY_EVENT(KEY_ESC);

        char seq[16] = {0};
        bool success = false;
        if (readConsoleKey(&c, timeout) < 0) {
            return result;
        }
        seq[0] = (char)c;

        if (seq[0] != '[') {
            // TODO: This is not always ALT
            result.key.value = KEY_EVENT(KEY_MOD_ALT, KEY_CHAR, seq[0]);
            return result;
        }

        for (size_t i = 1; i < sizeof(seq) - 1; i++) {
            if (readConsoleKey(&c, timeout) < 0) {
                return result;
            }
            seq[i] = (char)c;
            if (isUpper(seq[i]) || seq[i] == 'm' || seq[i] == '~') {
                success = true;
                break;
            }
        }

        if (!success) {
            return result;
        }

        // Bracketed paste
        if (strcmp(seq, "[200~") == 0) {
            VECTOR(Str) content = {0};
            abuf line = ABUF_INIT;

            bool last_was_cr = false;
            while (true) {
                if (readConsoleKey(&c, timeout) < 0) {
                    vector_free(content);
                    abufFree(&line);
                    return result;
                }

                if (c == '\x1b') {  // ESC
                    uint32_t end_seq[5];
                    bool is_end = true;
                    const char expected[5] = {'[', '2', '0', '1', '~'};

                    size_t index;
                    for (index = 0;
                         index < sizeof(end_seq) / sizeof(end_seq[0]);
                         index++) {
                        if (readConsoleKey(&end_seq[index], timeout) < 0) {
                            vector_free(content);
                            abufFree(&line);
                            return result;
                        }

                        if (end_seq[index] != (uint32_t)expected[index]) {
                            is_end = false;
                            break;
                        }
                    }

                    if (is_end) {
                        EditorClipboard clipboard = {0};
                        if (content.size || line.len) {
                            Str s_line = {
                                .data = line.buf,
                                .size = line.len,
                            };
                            vector_push(content, s_line);
                            vector_shrink(content);

                            // transfer the vector to clipboard
                            clipboard.size = content.size;
                            clipboard.lines = content.data;
                        }

                        result.type = EVENT_PASTE;
                        result.paste = clipboard;
                        return result;
                    }

                    // paste the escape sequence so far in
                    abufAppendN(&line, expected, index);
                    // let the rest of the logic handle the last input
                    c = end_seq[index];
                }

                if (c == '\r' || c == '\n') {
                    if (c == '\n' && last_was_cr) {
                        last_was_cr = false;
                        continue;
                    }

                    last_was_cr = (c == '\r');

                    Str s_line = {
                        .data = line.buf,
                        .size = line.len,
                    };
                    vector_push(content, s_line);
                    memset(&line, 0, sizeof(abuf));
                } else {
                    last_was_cr = false;

                    char utf8[4];
                    int bytes = encodeUTF8(c, utf8);
                    if (bytes == -1)
                        continue;
                    abufAppendN(&line, utf8, bytes);
                }
            }
        }

        // Mouse input
        if (seq[1] == '<') {
            // SGR: ESC [ < Cb ; Cx ; Cy (M|m)
            int Cb, Cx, Cy;
            char fin;
            if (!parseMouseSGR(&seq[1], &Cb, &Cx, &Cy, &fin)) {
                return result;
            }

            MouseEvent mouse_event = {
                .x = Cx - 1,
                .y = Cy - 1,
            };

            int btn = Cb & 0x03;  // 0=L, 1=M, 2=R
            bool motion = (Cb & 0x20) != 0;
            bool wheel = (Cb & 0x40) != 0;
            bool press = (fin == 'M');
            bool rel = (fin == 'm');

            if (wheel) {
                if ((Cb & 0x41) == 0x40) {
                    mouse_event.type = MWHEEL_UP;
                } else if ((Cb & 0x41) == 0x41) {
                    mouse_event.type = MWHEEL_DOWN;
                } else {
                    return result;
                }
            } else if (motion) {
                switch (btn) {
                    case 0:
                        mouse_event.type = MOUSE1_DRAG;
                        break;
                    case 1:
                        mouse_event.type = MOUSE2_DRAG;
                        break;
                    case 2:
                        mouse_event.type = MOUSE3_DRAG;
                        break;
                    default:
                        return result;
                }
            } else if (press) {
                switch (btn) {
                    case 0:
                        mouse_event.type = MOUSE1_PRESSED;
                        break;
                    case 1:
                        mouse_event.type = MOUSE2_PRESSED;
                        break;
                    case 2:
                        mouse_event.type = MOUSE3_PRESSED;
                        break;
                    default:
                        return result;
                }
            } else if (rel) {
                switch (btn) {
                    case 0:
                        mouse_event.type = MOUSE1_RELEASED;
                        break;
                    case 1:
                        mouse_event.type = MOUSE2_RELEASED;
                        break;
                    case 2:
                        mouse_event.type = MOUSE3_RELEASED;
                        break;
                    default:
                        return result;
                }
            }

            result.type = EVENT_MOUSE;
            result.mouse = mouse_event;
            return result;
        }

        for (size_t i = 0;
             i < sizeof(sequence_lookup) / sizeof(sequence_lookup[0]); i++) {
            if (strcmp(sequence_lookup[i].str, seq) == 0) {
                result.type = EVENT_KEY;
                result.key.value = sequence_lookup[i].value;
                return result;
            }
        }
        return result;
    }

    result.type = EVENT_KEY;

    if (c == '\r') {
        result.key.value = KEY_EVENT(KEY_ENTER);
        return result;
    }

    if (c == '\t') {
        result.key.value = KEY_EVENT(KEY_TAB);
        return result;
    }

    if (c == 127) {
        result.key.value = KEY_EVENT(KEY_BACKSPACE);
        return result;
    }

    if (c < 32) {
        result.key.value = KEY_EVENT(KEY_MOD_CTRL, KEY_CHAR, c + 0x40);
        return result;
    }

    result.key.value = KEY_EVENT(KEY_TEXT);
    result.key.unicode = c;
    return result;
}

void eventFree(Event* event) {
    if (!event)
        return;
    if (event->type == EVENT_PASTE) {
        editorFreeClipboardContent(&event->paste);
    }
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

#define ANSI_SWAP_ENABLE "\x1b[?1049h"
#define ANSI_SWAP_DISABLE "\x1b[?1049l"
#define ANSI_MOUSE_ENABLE "\x1b[?1000h\x1b[?1002h\x1b[?1006h"
#define ANSI_MOUSE_DISABLE "\x1b[?1007l\x1b[?1006l\x1b[?1002l\x1b[?1000l"
#define ANSI_BRACKETED_PASTE_ENABLE "\x1b[?2004h"
#define ANSI_BRACKETED_PASTE_DISABLE "\x1b[?2004l"

void enableMouse(void) {
    writeConsoleStr(ANSI_MOUSE_ENABLE);
}

void disableMouse(void) {
    writeConsoleStr(ANSI_MOUSE_DISABLE);
}

void terminalInit(void) {
    terminalStart();

    if (signal(SIGSEGV, SIGSEGV_handler) == SIG_ERR) {
        PANIC("Failed to install SIGSEGV handler");
    }

    if (signal(SIGABRT, SIGABRT_handler) == SIG_ERR) {
        PANIC("Failed to install SIGABRT handler");
    }
}

static bool terminal_active = false;

void terminalStart(void) {
    terminal_active = true;
    enableRawMode();
    writeConsoleStr(ANSI_SWAP_ENABLE ANSI_BRACKETED_PASTE_ENABLE);
    if (gEditor.mouse_mode) {
        enableMouse();
    } else {
        disableMouse();
    }

    editorResizeWindow();
}

void terminalExit(void) {
    if (!terminal_active)
        return;
    terminal_active = false;
    writeConsoleStr(ANSI_MOUSE_DISABLE ANSI_BRACKETED_PASTE_DISABLE
                        ANSI_SWAP_DISABLE ANSI_CLEAR_STYLE ANSI_CURSOR_SHOW);
    disableRawMode();
}
