#define _GNU_SOURCE  // SIGWINCH

#include "terminal.h"

#include <ctype.h>
#include <signal.h>

#include "config.h"
#include "editor.h"
#include "os.h"
#include "output.h"
#include "unicode.h"
#include "utils.h"

typedef struct {
    const char* str;
    int value;
} StrIntPair;

static const StrIntPair sequence_lookup[] = {
    {"[1~", HOME_KEY},
    // {"[2~", INSERT_KEY},
    {"[3~", DEL_KEY},
    {"[4~", END_KEY},
    {"[5~", PAGE_UP},
    {"[6~", PAGE_DOWN},
    {"[7~", HOME_KEY},
    {"[8~", END_KEY},

    {"[A", ARROW_UP},
    {"[B", ARROW_DOWN},
    {"[C", ARROW_RIGHT},
    {"[D", ARROW_LEFT},
    {"[F", END_KEY},
    {"[H", HOME_KEY},

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
    {"[1;2A", SHIFT_UP},
    {"[1;2B", SHIFT_DOWN},
    {"[1;2C", SHIFT_RIGHT},
    {"[1;2D", SHIFT_LEFT},
    {"[1;2F", SHIFT_END},
    {"[1;2H", SHIFT_HOME},

    // Alt
    {"[1;3A", ALT_UP},
    {"[1;3B", ALT_DOWN},

    // Shift+Alt
    {"[1;4A", SHIFT_ALT_UP},
    {"[1;4B", SHIFT_ALT_DOWN},

    // Ctrl
    {"[1;5A", CTRL_UP},
    {"[1;5B", CTRL_DOWN},
    {"[1;5C", CTRL_RIGHT},
    {"[1;5D", CTRL_LEFT},
    {"[1;5F", CTRL_END},
    {"[1;5H", CTRL_HOME},

    // Shift+Ctrl
    {"[1;6A", SHIFT_CTRL_UP},
    {"[1;6B", SHIFT_CTRL_DOWN},
    {"[1;6C", SHIFT_CTRL_RIGHT},
    {"[1;6D", SHIFT_CTRL_LEFT},

    // Page UP / Page Down
    {"[5;2~", SHIFT_PAGE_UP},
    {"[6;2~", SHIFT_PAGE_DOWN},
    {"[5;5~", CTRL_PAGE_UP},
    {"[6;5~", CTRL_PAGE_DOWN},
    {"[5;6~", SHIFT_CTRL_PAGE_UP},
    {"[6;6~", SHIFT_CTRL_PAGE_DOWN},
};

EditorInput editorReadKey(void) {
    uint32_t c;
    EditorInput result = {.type = UNKNOWN};

    while (!readConsole(&c, READ_WAIT_INFINITE)) {
    }

    int timeout = CONVAR_GETINT(ttimeoutlen);

    if (c == ESC) {
        char seq[16] = {0};
        bool success = false;
        if (!readConsole(&c, timeout)) {
            result.type = ESC;
            return result;
        }
        seq[0] = (char)c;

        if (seq[0] != '[') {
            result.type = ALT_KEY(seq[0]);
            return result;
        }

        for (size_t i = 1; i < sizeof(seq) - 1; i++) {
            if (!readConsole(&c, timeout)) {
                return result;
            }
            seq[i] = (char)c;
            if (isupper((uint8_t)seq[i]) || seq[i] == 'm' || seq[i] == '~') {
                success = true;
                break;
            }
        }

        if (!success) {
            return result;
        }

        if (strcmp(seq, "[200~") == 0) {
            VECTOR(Str) content = {0};
            abuf line = ABUF_INIT;

            bool last_was_cr = false;
            while (true) {
                if (!readConsole(&c, timeout)) {
                    free(content.data);
                    abufFree(&line);
                    return result;
                }

                if (c == ESC) {
                    uint32_t end_seq[5];
                    bool is_end = true;
                    const char expected[5] = {'[', '2', '0', '1', '~'};

                    size_t index;
                    for (index = 0;
                         index < sizeof(end_seq) / sizeof(end_seq[0]);
                         index++) {
                        if (!readConsole(&end_seq[index], timeout)) {
                            free(content.data);
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

                            clipboard.size = content.size;
                            clipboard.lines = content.data;
                        }

                        result.type = PASTE_INPUT;
                        result.data.paste = clipboard;
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
        if (seq[1] == '<' && gEditor.mouse_mode) {
            int type;
            char m;
            sscanf(&seq[2], "%d;%d;%d%c", &type, &result.data.cursor.x,
                   &result.data.cursor.y, &m);
            (*&result.data.cursor.x)--;
            (*&result.data.cursor.y)--;

            switch (type) {
                case 0:
                    if (m == 'M') {
                        result.type = MOUSE_PRESSED;
                    } else if (m == 'm') {
                        result.type = MOUSE_RELEASED;
                    }
                    break;
                case 1:
                    if (m == 'M') {
                        result.type = SCROLL_PRESSED;
                    } else if (m == 'm') {
                        result.type = SCROLL_RELEASED;
                    }
                    break;
                case 32:
                    result.type = MOUSE_MOVE;
                    break;
                case 64:
                    result.type = WHEEL_UP;
                    break;
                case 65:
                    result.type = WHEEL_DOWN;
                    break;
                default:
                    break;
            }
            return result;
        }

        for (size_t i = 0;
             i < sizeof(sequence_lookup) / sizeof(sequence_lookup[0]); i++) {
            if (strcmp(sequence_lookup[i].str, seq) == 0) {
                result.type = sequence_lookup[i].value;
                return result;
            }
        }
        return result;
    }

    if ((c <= 31 || c == BACKSPACE) && c != '\t') {
        result.type = c;
        return result;
    }

    result.type = CHAR_INPUT;
    result.data.unicode = c;

    return result;
}

void editorFreeInput(EditorInput* input) {
    if (!input)
        return;
    if (input->type == PASTE_INPUT) {
        editorFreeClipboardContent(&input->data.paste);
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

#define SWAP_ENABLE "\x1b[?1049h"
#define SWAP_DISABLE "\x1b[?1049l"
#define MOUSE_ENABLE "\x1b[?1002h\x1b[?1015h\x1b[?1006h"
#define MOUSE_DISABLE "\x1b[?1002l\x1b[?1015l\x1b[?1006l"
#define BRACKETED_PASTE_ENABLE "\x1b[?2004h"
#define BRACKETED_PASTE_DISABLE "\x1b[?2004l"

void enableMouse(void) {
    if (!gEditor.mouse_mode) {
        writeConsoleStr(MOUSE_ENABLE);
        gEditor.mouse_mode = true;
    }
}

void disableMouse(void) {
    if (gEditor.mouse_mode) {
        writeConsoleStr(MOUSE_DISABLE);
        gEditor.mouse_mode = false;
    }
}

#ifndef _WIN32
static void SIGWINCH_handler(int sig) {
    if (sig != SIGWINCH)
        return;
    resizeWindow();
}
#endif

void resizeWindow(void) {
    int rows = 0;
    int cols = 0;

    if (getWindowSize(&rows, &cols) == -1)
        PANIC("getWindowSize");
    setWindowSize(rows, cols);
}

void setWindowSize(int rows, int cols) {
    rows = rows < 1 ? 1 : rows;
    cols = cols < 1 ? 1 : cols;

    if (gEditor.screen_rows != rows || gEditor.screen_cols != cols) {
        gEditor.screen_rows = rows;
        gEditor.screen_cols = cols;
        // TODO: Don't hard coding rows
        gEditor.display_rows = (rows < 2) ? 0 : rows - 2;

        if (!gEditor.loading)
            editorRefreshScreen();
    }
}

void editorInitTerminal(void) {
    enableRawMode();
    writeConsoleStr(SWAP_ENABLE BRACKETED_PASTE_ENABLE MOUSE_ENABLE);
    atexit(terminalExit);
    resizeWindow();

    if (signal(SIGSEGV, SIGSEGV_handler) == SIG_ERR) {
        PANIC("SIGSEGV_handler");
    }

    if (signal(SIGABRT, SIGABRT_handler) == SIG_ERR) {
        PANIC("SIGABRT_handler");
    }

#ifndef _WIN32
    if (signal(SIGWINCH, SIGWINCH_handler) == SIG_ERR) {
        PANIC("SIGWINCH_handler");
    }
#endif
}

void terminalExit(void) {
    writeConsoleStr(MOUSE_DISABLE BRACKETED_PASTE_DISABLE SWAP_DISABLE
                        ANSI_CLEAR ANSI_CURSOR_SHOW);
    disableRawMode();
}
