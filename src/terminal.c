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

void die(char* file, int line, const char* s) {
    disableSwap();
    fprintf(stderr, "Error at %s: %d: %s\r\n", file, line, s);
    exit(EXIT_FAILURE);
}

static void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        DIE("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
        DIE("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        DIE("tcsetattr");
}

int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            DIE("read");

        // Auto resize
        resizeWindow();
    }
    if (c == ESC) {
        char seq[5];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return ESC;
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return ESC;
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1)
                    return ESC;
                if (seq[2] == '~') {
                    switch (seq[1]) {
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
                } else if (seq[2] == ';') {
                    if (read(STDIN_FILENO, &seq[3], 1) != 1)
                        return ESC;
                    if (read(STDIN_FILENO, &seq[4], 1) != 1)
                        return ESC;
                    if (seq[1] == '1') {
                        if (seq[3] == '2') {
                            // Shift
                            switch (seq[4]) {
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
                        } else if (seq[3] == '5') {
                            // Ctrl
                            switch (seq[4]) {
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
                        } else if (seq[3] == '6') {
                            // Shift + Ctrl
                            switch (seq[4]) {
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
                    }
                }
            } else {
                switch (seq[1]) {
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
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H':
                    return HOME_KEY;
                case 'F':
                    return END_KEY;
            }
        }
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

static void handler(int sig) {
    disableSwap();
    disableRawMode();
    write(STDOUT_FILENO, "Segmentation fault\r\n", 20) == 20;
    _exit(EXIT_FAILURE);
}

int enableSwap() {
    if (signal(SIGSEGV, handler) == SIG_ERR)
        return 0;
    return write(STDOUT_FILENO, "\x1b[?1049h\x1b[H", 11) == 11;
}

int disableSwap() { return write(STDOUT_FILENO, "\x1b[?1049l", 8) == 8; }

void resizeWindow() {
    int rows, cols;
    if (getWindowSize(&rows, &cols) == -1)
        DIE("getWindowSize");

    if (E.screen_rows != rows || E.screen_cols != cols) {
        E.screen_rows = rows;
        E.screen_cols = cols;
        E.rows = rows - 3;
        E.cols = cols - (E.num_rows_digits + 1);
        editorRefreshScreen();
    }
}
