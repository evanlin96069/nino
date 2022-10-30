#include "input.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "defines.h"
#include "editor.h"
#include "file_io.h"
#include "find.h"
#include "goto_line.h"
#include "output.h"
#include "select.h"
#include "status.h"
#include "terminal.h"
#include "utils.h"

char* editorPrompt(char* prompt, int state, void (*callback)(char*, int)) {
    int prev_state = E.state;
    E.state = state;

    size_t bufsize = 128;
    char* buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    int start = 0;
    while (prompt[start] != '\0' && prompt[start] != '%') {
        start++;
    }
    E.px = start;
    while (1) {
        editorSetStatusMsg(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        switch (c) {
            case DEL_KEY:
            case CTRL_KEY('h'):
            case BACKSPACE:
                if (buflen != 0) {
                    buf[--buflen] = '\0';
                    E.px--;
                    if (callback)
                        callback(buf, c);
                }
                break;

            case CTRL_KEY('q'):
            case ESC:
                editorSetStatusMsg("");
                free(buf);
                E.state = prev_state;
                if (callback)
                    callback(buf, c);
                return NULL;

            case '\r':
                if (buflen != 0) {
                    editorSetStatusMsg("");
                    E.state = prev_state;
                    if (callback)
                        callback(buf, c);
                    return buf;
                }
                break;

            default:
                if (!iscntrl(c) && c < 128) {
                    if (buflen == bufsize - 1) {
                        bufsize *= 2;
                        buf = realloc(buf, bufsize);
                    }
                    buf[buflen++] = c;
                    buf[buflen] = '\0';
                    E.px++;
                }
                if (callback)
                    callback(buf, c);
                break;
        }
        if (getWindowSize(&E.rows, &E.cols) == -1)
            DIE("getWindowSize");
        E.rows -= 3;
        E.cols -= E.num_rows_digits + 1;
    }
}

void editorMoveCursor(int key) {
    EditorRow* row = (E.cy >= E.num_rows) ? NULL : &(E.row[E.cy]);
    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
                E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
                E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
                E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            } else if (row && (E.cy + 1 < E.num_rows) && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
                E.sx = 0;
            }

            break;
        case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
                E.cx = editorRowSxToCx(&(E.row[E.cy]), E.sx);
            }
            break;
        case ARROW_DOWN:
            if (E.cy + 1 < E.num_rows) {
                E.cy++;
                E.cx = editorRowSxToCx(&(E.row[E.cy]), E.sx);
            }
            break;
    }
    row = (E.cy >= E.num_rows) ? NULL : &(E.row[E.cy]);
    int row_len = row ? row->size : 0;
    if (E.cx > row_len) {
        E.cx = row_len;
    }
}

static char isOpenBracket(int key) {
    switch (key) {
        case '(':
            return ')';
        case '[':
            return ']';
        case '{':
            return '}';
        default:
            return 0;
    }
}

static char isCloseBracket(int key) {
    switch (key) {
        case ')':
            return '(';
        case ']':
            return '[';
        case '}':
            return '{';
        default:
            return 0;
    }
}

void editorProcessKeypress() {
    static int quit_protect = 1;
    int c = editorReadKey();
    editorSetStatusMsg("");
    switch (c) {
        case '\r':
            if (E.is_selected) {
                editorDeleteSelectText();
                E.is_selected = 0;
            }
            E.bracket_autocomplete = 0;

            editorInsertNewline();
            break;

        case CTRL_KEY('q'):
            if (E.dirty && quit_protect) {
                editorSetStatusMsg(
                    "File has unsaved changes. Press ^Q again to quit anyway.");
                quit_protect = 0;
                return;
            }
            editorFree();
            disableSwap();
            exit(EXIT_SUCCESS);
            break;

        case CTRL_KEY('s'):
            if (E.dirty)
                editorSave();
            break;

        case HOME_KEY:
        case CTRL_LEFT:
        case SHIFT_HOME:
        case SHIFT_CTRL_LEFT:
            if (E.cx == 0)
                break;
            E.cx = 0;
            E.sx = 0;
            E.is_selected = (c == SHIFT_HOME || c == SHIFT_CTRL_LEFT);
            E.bracket_autocomplete = 0;
            break;
        case END_KEY:
        case CTRL_RIGHT:
        case SHIFT_END:
        case SHIFT_CTRL_RIGHT:
            if (E.cy < E.num_rows && E.cx != E.row[E.cy].size) {
                E.cx = E.row[E.cy].size;
                E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
                E.is_selected = (c == SHIFT_END || c == SHIFT_CTRL_RIGHT);
                E.bracket_autocomplete = 0;
            }
            break;

        case CTRL_KEY('f'):
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorFind();
            break;

        case CTRL_KEY('g'):
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorGotoLine();
            break;

        case CTRL_KEY('p'):
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorSetting();
            break;

        case CTRL_KEY('a'):
            if (E.num_rows == 1 && E.row[0].size == 0)
                break;
            E.is_selected = 1;
            E.bracket_autocomplete = 0;
            E.cy = E.num_rows - 1;
            E.cx = E.row[E.num_rows - 1].size;
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            E.select_y = 0;
            E.select_x = 0;
            break;

        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE:
            if (E.is_selected) {
                editorDeleteSelectText();
                E.is_selected = 0;
                break;
            }
            if (c == DEL_KEY)
                editorMoveCursor(ARROW_RIGHT);
            else if (E.bracket_autocomplete &&
                     (isCloseBracket(E.row[E.cy].data[E.cx]) ==
                          E.row[E.cy].data[E.cx - 1] ||
                      (E.row[E.cy].data[E.cx] == '\'' &&
                       E.row[E.cy].data[E.cx - 1] == '\'') ||
                      (E.row[E.cy].data[E.cx] == '"' &&
                       E.row[E.cy].data[E.cx - 1] == '"'))) {
                E.bracket_autocomplete--;
                editorMoveCursor(ARROW_RIGHT);
                editorDelChar();
            }
            char deleted_char = E.row[E.cy].data[E.cx - 1];
            editorDelChar();
            if (deleted_char == ' ') {
                int should_delete_tab = 1;
                for (int i = 0; i < E.cx; i++) {
                    if (!isspace(E.row[E.cy].data[i])) {
                        should_delete_tab = 0;
                    }
                }
                if (should_delete_tab) {
                    int idx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
                    while (idx % E.cfg->tab_size != 0) {
                        editorDelChar();
                        idx--;
                    }
                }
            }
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            {
                if (c == PAGE_UP) {
                    E.cy = E.row_offset;
                } else if (c == PAGE_DOWN) {
                    E.cy = E.row_offset + E.rows - 1;
                    if (E.cy >= E.num_rows)
                        E.cy = E.num_rows - 1;
                }
                int times = E.rows;
                while (times--) {
                    if (c == PAGE_UP) {
                        if (E.cy == 0) {
                            E.cx = 0;
                            E.sx = 0;
                            break;
                        }
                        editorMoveCursor(ARROW_UP);
                    } else {
                        if (E.cy == E.num_rows - 1) {
                            E.cx = E.row[E.cy].size;
                            break;
                        }
                        editorMoveCursor(ARROW_DOWN);
                    }
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            if (E.is_selected) {
                int start_x, start_y, end_x, end_y;
                getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);

                if (c == ARROW_UP || c == ARROW_LEFT) {
                    E.cx = start_x;
                    E.cy = start_y;
                } else {
                    E.cx = end_x;
                    E.cy = end_y;
                }
                E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
                if (c == ARROW_UP || c == ARROW_DOWN) {
                    editorMoveCursor(c);
                }
                E.is_selected = 0;
            } else {
                if (E.bracket_autocomplete) {
                    if (ARROW_RIGHT)
                        E.bracket_autocomplete--;
                    else
                        E.bracket_autocomplete = 0;
                }
                editorMoveCursor(c);
            }
            break;

        case SHIFT_UP:
        case SHIFT_DOWN:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            E.is_selected = 1;
            E.bracket_autocomplete = 0;
            editorMoveCursor(c - 9);
            break;

        case CTRL_UP:
        case CTRL_HOME:
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            E.cy = 0;
            E.cx = 0;
            E.sx = 0;
            break;
        case CTRL_DOWN:
        case CTRL_END:
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            E.cy = E.num_rows - 1;
            E.cx = E.row[E.num_rows - 1].size;
            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            break;

        case SHIFT_CTRL_UP:
        case SHIFT_CTRL_DOWN:

        case CTRL_KEY('l'):
        case ESC:
            break;

        default:
            if (isprint(c) || c == '\t') {
                if (E.is_selected) {
                    editorDeleteSelectText();
                    E.is_selected = 0;
                }
                int close_bracket = isOpenBracket(c);
                int open_bracket = isCloseBracket(c);
                if (close_bracket) {
                    editorInsertChar(c);
                    editorInsertChar(close_bracket);
                    E.cx--;
                    E.bracket_autocomplete++;
                } else if (open_bracket) {
                    if (E.bracket_autocomplete && E.row[E.cy].data[E.cx] == c) {
                        E.bracket_autocomplete--;
                        E.cx++;
                    } else {
                        editorInsertChar(c);
                    }
                } else if (c == '\'' || c == '"') {
                    if (E.row[E.cy].data[E.cx] != c) {
                        editorInsertChar(c);
                        editorInsertChar(c);
                        E.cx--;
                        E.bracket_autocomplete++;
                    } else if (E.bracket_autocomplete &&
                               E.row[E.cy].data[E.cx] == c) {
                        E.bracket_autocomplete--;
                        E.cx++;
                    } else {
                        editorInsertChar(c);
                    }
                } else {
                    editorInsertChar(c);
                }
                E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            }
            E.is_selected = 0;
            break;
    }
    if (!E.is_selected) {
        E.select_x = E.cx;
        E.select_y = E.cy;
    }
    quit_protect = 1;
}
