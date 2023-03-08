#include "terminal.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "defines.h"
#include "editor.h"
#include "output.h"

void panic(char* file, int line, const char* s) {
    terminalExit();
    fprintf(stderr, "Error at %s: %d: %s\r\n", file, line, s);
    exit(EXIT_FAILURE);
}

static void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &gEditor.orig_termios) == -1)
        PANIC("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &gEditor.orig_termios) == -1)
        PANIC("tcgetattr");

    struct termios raw = gEditor.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        PANIC("tcsetattr");
}

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

int editorReadKey(int* x, int* y) {
    int nread;
    char c;

    *x = *y = 0;

    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            PANIC("read");
    }

    if (c == ESC) {
        char seq[16] = {0};
        bool success = false;
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return ESC;
        if (seq[0] != '[')
            return ALT_KEY(seq[0]);
        for (size_t i = 1; i < sizeof(seq) - 1; i++) {
            if (read(STDIN_FILENO, &seq[i], 1) != 1)
                return UNKNOWN;
            if (isupper(seq[i]) || seq[i] == 'm' || seq[i] == '~') {
                success = true;
                break;
            }
        }

        if (!success)
            return UNKNOWN;

        // Mouse input
        if (seq[1] == '<' && gEditor.mouse_mode) {
            int type;
            char m;
            sscanf(&seq[2], "%d;%d;%d%c", &type, x, y, &m);
            (*x)--;
            (*y)--;
            switch (type) {
                case 0:
                    if (m == 'M')
                        return MOUSE_PRESSED;
                    if (m == 'm')
                        return MOUSE_RELEASED;
                    return UNKNOWN;
                case 1:
                    if (m == 'M')
                        return SCROLL_PRESSED;
                    if (m == 'm')
                        return SCROLL_RELEASED;
                    return UNKNOWN;
                case 32:
                    return MOUSE_MOVE;
                case 64:
                    return WHEEL_UP;
                case 65:
                    return WHEEL_DOWN;
                default:
                    return UNKNOWN;
            }
        }

        for (size_t i = 0;
             i < sizeof(sequence_lookup) / sizeof(sequence_lookup[0]); i++) {
            if (strcmp(sequence_lookup[i].str, seq) == 0) {
                return sequence_lookup[i].value;
            }
        }
        return UNKNOWN;
    }
    return c;
}

static int getCursorPos(int* rows, int* cols) {
    char buf[32];
    size_t i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
        return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1)
            break;
        if (buf[i] == 'R')
            break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[')
        return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2)
        return -1;
    return 0;
}

int getWindowSize(int* rows, int* cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
            return -1;
        return getCursorPos(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

static void SIGSEGV_handler(int sig) {
    if (sig != SIGSEGV)
        return;
    terminalExit();
    UNUSED(write(STDOUT_FILENO, "Exit from SIGSEGV_handler\r\n", 20));
    _exit(EXIT_FAILURE);
}

void enableSwap() {
    if (signal(SIGSEGV, SIGSEGV_handler) == SIG_ERR)
        return;
    UNUSED(write(STDOUT_FILENO, "\x1b[?1049h\x1b[H", 11));
}

void disableSwap() { UNUSED(write(STDOUT_FILENO, "\x1b[?1049l", 8)); }

void enableMouse() {
    if (!gEditor.mouse_mode &&
        write(STDOUT_FILENO, "\x1b[?1002h\x1b[?1015h\x1b[?1006h", 24) == 24)
        gEditor.mouse_mode = true;
}

void disableMouse() {
    if (gEditor.mouse_mode &&
        write(STDOUT_FILENO, "\x1b[?1002l\x1b[?1015l\x1b[?1006l", 24) == 24)
        gEditor.mouse_mode = false;
}

static void SIGWINCH_handler(int sig) {
    if (sig != SIGWINCH)
        return;
    resizeWindow();
}

void enableAutoResize() { signal(SIGWINCH, SIGWINCH_handler); }

void resizeWindow() {
    int rows, cols;
    if (getWindowSize(&rows, &cols) == -1)
        PANIC("getWindowSize");

    if (gEditor.screen_rows != rows || gEditor.screen_cols != cols) {
        gEditor.screen_rows = rows;
        gEditor.screen_cols = cols;
        // TODO: Don't hard coding rows
        gEditor.display_rows = rows - 3;
        editorRefreshScreen();
    }
}

void terminalExit() {
    disableRawMode();
    disableSwap();
    disableMouse();
    // Show cursor
    UNUSED(write(STDOUT_FILENO, "\x1b[?25h", 6));
}
