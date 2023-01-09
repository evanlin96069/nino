#include "terminal.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        PANIC("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        PANIC("tcgetattr");

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        PANIC("tcsetattr");
}

int editorReadKey(int* x, int* y) {
    int nread;
    char seq[32];

    *x = *y = 0;

    while ((nread = read(STDIN_FILENO, &seq, sizeof(seq))) == 0) {
    }

    if (nread == -1 && errno != EAGAIN)
        PANIC("read");

    if (seq[0] == ESC) {
        if (nread < 3)
            return ESC;

        // TODO: Use a map?
        if (seq[1] == '[') {
            if (seq[2] >= '0' && seq[2] <= '9') {
                if (nread < 4)
                    return ESC;

                if (seq[3] == '~') {
                    switch (seq[2]) {
                        case '1':
                            return HOME_KEY;
                        case '3':
                            return DEL_KEY;
                        case '4':
                            return END_KEY;
                        case '5':
                            return PAGE_UP;
                        case '6':
                            return PAGE_DOWN;
                        case '7':
                            return HOME_KEY;
                        case '8':
                            return END_KEY;
                    }
                } else if (seq[3] == ';') {
                    if (nread < 6)
                        return ESC;

                    if (seq[2] == '1') {
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
                        if (seq[4] == '2') {
                            // Shift
                            switch (seq[5]) {
                                case 'A':
                                    return SHIFT_UP;
                                case 'B':
                                    return SHIFT_DOWN;
                                case 'C':
                                    return SHIFT_RIGHT;
                                case 'D':
                                    return SHIFT_LEFT;
                                case 'H':
                                    return SHIFT_HOME;
                                case 'F':
                                    return SHIFT_END;
                            }
                        } else if (seq[4] == '3') {
                            // Alt
                            switch (seq[5]) {
                                case 'A':
                                    return ALT_UP;
                                case 'B':
                                    return ALT_DOWN;
                            }
                        } else if (seq[4] == '4') {
                            // Shift + Alt
                            switch (seq[5]) {
                                case 'A':
                                    return SHIFT_ALT_UP;
                                case 'B':
                                    return SHIFT_ALT_DOWN;
                            }
                        } else if (seq[4] == '5') {
                            // Ctrl
                            switch (seq[5]) {
                                case 'A':
                                    return CTRL_UP;
                                case 'B':
                                    return CTRL_DOWN;
                                case 'C':
                                    return CTRL_RIGHT;
                                case 'D':
                                    return CTRL_LEFT;
                                case 'H':
                                    return CTRL_HOME;
                                case 'F':
                                    return CTRL_END;
                            }
                        } else if (seq[4] == '6') {
                            // Shift + Ctrl
                            switch (seq[5]) {
                                case 'A':
                                    return SHIFT_CTRL_UP;
                                case 'B':
                                    return SHIFT_CTRL_DOWN;
                                case 'C':
                                    return SHIFT_CTRL_RIGHT;
                                case 'D':
                                    return SHIFT_CTRL_LEFT;
                            }
                        }
                    } else if (seq[2] == '5' && seq[5] == '~') {
                        // PageUp
                        switch (seq[4]) {
                            case '2':
                                return SHIFT_PAGE_UP;
                            case '5':
                                return CTRL_PAGE_UP;
                            case '6':
                                return SHIFT_CTRL_PAGE_UP;
                        }
                    } else if (seq[2] == '6' && seq[5] == '~') {
                        // PageDn
                        switch (seq[4]) {
                            case '2':
                                return SHIFT_PAGE_DOWN;
                            case '5':
                                return CTRL_PAGE_DOWN;
                            case '6':
                                return SHIFT_CTRL_PAGE_DOWN;
                        }
                    }
                }
            } else if (seq[2] == '<' && E.mouse_mode) {
                // Mouse input
                char pos[16] = {0};
                int idx = 0;
                for (int i = 3; i < nread; i++) {
                    pos[i - 3] = seq[i];
                    if (seq[i] == 'm' || seq[i] == 'M')
                        break;
                }
                pos[15] = '\0';

                int type;
                char m;
                sscanf(pos, "%d;%d;%d%c", &type, x, y, &m);
                (*x)--;
                (*y)--;
                switch (type) {
                    case 0:
                        if (m == 'M')
                            return MOUSE_PRESSED;
                        if (m == 'm')
                            return MOUSE_RELEASED;
                    case 1:
                        if (m == 'M')
                            return SCROLL_PRESSED;
                        if (m == 'm')
                            return SCROLL_RELEASED;
                        break;
                    case 32:
                        return MOUSE_MOVE;
                    case 64:
                        return WHEEL_UP;
                    case 65:
                        return WHEEL_DOWN;
                }
            } else {
                switch (seq[2]) {
                    case 'A':
                        return ARROW_UP;
                    case 'B':
                        return ARROW_DOWN;
                    case 'C':
                        return ARROW_RIGHT;
                    case 'D':
                        return ARROW_LEFT;
                    case 'H':
                        return HOME_KEY;
                    case 'F':
                        return END_KEY;
                }
            }
        } else if (seq[1] == 'O') {
            switch (seq[2]) {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }
    }
    return seq[0];
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
    if (!E.mouse_mode &&
        write(STDOUT_FILENO, "\x1b[?1002h\x1b[?1015h\x1b[?1006h", 24) == 24)
        E.mouse_mode = 1;
}

void disableMouse() {
    if (E.mouse_mode && write(STDOUT_FILENO, "\x1b[?1002l", 8) == 8)
        E.mouse_mode = 0;
}

static void SIGWINCH_handler(int sig) { resizeWindow(); }

void enableAutoResize() { signal(SIGWINCH, SIGWINCH_handler); }

void resizeWindow() {
    int rows, cols;
    if (getWindowSize(&rows, &cols) == -1)
        PANIC("getWindowSize");

    if (E.screen_rows != rows || E.screen_cols != cols) {
        E.screen_rows = rows;
        E.screen_cols = cols;
        E.rows = rows - 3;
        E.cols = cols - (E.num_rows_digits + 1);
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