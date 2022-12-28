#include "input.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
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
        int x, y;
        int c = editorReadKey(&x, &y);
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
            PANIC("getWindowSize");
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

static int isValidMousePos(int x, int y) {
    if (y < 1 || y >= E.screen_rows - 2)
        return 0;
    if (x < E.num_rows_digits + 1)
        return 0;
    return 1;
}

static int mousePosToEditorPos(int* x, int* y) {
    int row = E.row_offset + *y - 1;
    if (row >= E.num_rows)
        return 0;
    int col = *x - E.num_rows_digits - 1 + E.col_offset;
    if (col > E.row[row].rsize)
        col = E.row[row].rsize;
    *x = col;
    *y = row;
    return 1;
}

void editorProcessKeypress() {
    static int quit_protect = 1;
    static int pressed = 0;
    static int prev_x = 0;
    static int prev_y = 0;

    int x, y;
    int c = editorReadKey(&x, &y);

    int should_scroll = 1;

    editorSetStatusMsg("");
    switch (c) {
        // Action: Newline
        case '\r': {
            EditorAction* action = malloc(sizeof(EditorAction));
            if (!action)
                PANIC("malloc");

            getSelectStartEnd(&action->deleted_range);

            if (E.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.is_selected = 0;
            } else {
                action->deleted_text.size = 0;
                action->deleted_text.data = NULL;
                action->deleted_range = (EditorSelectRange){0};
            }

            E.bracket_autocomplete = 0;

            action->added_range.start_x = E.cx;
            action->added_range.start_y = E.cy;
            editorInsertNewline();
            action->added_range.end_x = E.cx;
            action->added_range.end_y = E.cy;
            editorCopyText(&action->added_text, action->added_range);

            editorAppendAction(action);
        } break;

        case CTRL_KEY('q'):
            if (E.dirty && quit_protect) {
                editorSetStatusMsg(
                    "File has unsaved changes. Press ^Q again to quit anyway.");
                quit_protect = 0;
                return;
            }
            editorFree();
            exit(EXIT_SUCCESS);
            break;

        case CTRL_KEY('s'):
            should_scroll = 0;
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

        // Action: Delete
        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE: {
            EditorAction* action = malloc(sizeof(EditorAction));
            if (!action)
                PANIC("malloc");
            action->added_text.size = 0;
            action->added_text.data = NULL;
            action->added_range = (EditorSelectRange){0};

            if (E.is_selected) {
                getSelectStartEnd(&action->deleted_range);
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorAppendAction(action);
                editorDeleteText(action->deleted_range);
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
                editorMoveCursor(ARROW_RIGHT);
            }

            action->deleted_range.end_x = E.cx;
            action->deleted_range.end_y = E.cy;

            if (E.bracket_autocomplete) {
                E.bracket_autocomplete--;
                editorMoveCursor(ARROW_LEFT);
            }
            char deleted_char = E.row[E.cy].data[E.cx - 1];
            editorMoveCursor(ARROW_LEFT);
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
                        editorMoveCursor(ARROW_LEFT);
                        idx--;
                    }
                }
            }
            action->deleted_range.start_x = E.cx;
            action->deleted_range.start_y = E.cy;
            editorCopyText(&action->deleted_text, action->deleted_range);
            editorDeleteText(action->deleted_range);

            editorAppendAction(action);
        } break;

        // Action: Cut
        case CTRL_KEY('x'): {
            if (!E.is_selected)
                break;
            EditorAction* action = malloc(sizeof(EditorAction));
            if (!action)
                PANIC("malloc");
            action->added_text.size = 0;
            action->added_text.data = NULL;
            action->added_range = (EditorSelectRange){0};

            getSelectStartEnd(&action->deleted_range);
            editorCopyText(&action->deleted_text, action->deleted_range);
            editorAppendAction(action);

            editorFreeClipboardContent(&E.clipboard);
            editorCopyText(&E.clipboard, action->deleted_range);
            editorDeleteText(action->deleted_range);
            E.is_selected = 0;
        } break;

        // Copy
        case CTRL_KEY('c'): {
            if (!E.is_selected)
                return;

            EditorSelectRange range;
            getSelectStartEnd(&range);
            editorFreeClipboardContent(&E.clipboard);
            editorCopyText(&E.clipboard, range);
            should_scroll = 0;
        } break;

        // Action: Paste
        case CTRL_KEY('v'): {
            if (!E.clipboard.size)
                break;

            EditorAction* action = malloc(sizeof(EditorAction));
            if (!action)
                PANIC("malloc");

            getSelectStartEnd(&action->deleted_range);

            if (E.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.is_selected = 0;
            } else {
                action->deleted_text.size = 0;
                action->deleted_text.data = NULL;
            }

            action->added_range.start_x = E.cx;
            action->added_range.start_y = E.cy;
            editorPasteText(&E.clipboard, E.cx, E.cy);
            action->added_range.end_x = E.cx;
            action->added_range.end_y = E.cy;
            editorCopyText(&action->added_text, action->added_range);
            editorAppendAction(action);
        } break;

        // Undo
        case CTRL_KEY('z'):
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorUndo();
            break;

        // Redo
        case CTRL_KEY('y'):
            E.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorRedo();
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
                EditorSelectRange range;
                getSelectStartEnd(&range);

                if (c == ARROW_UP || c == ARROW_LEFT) {
                    E.cx = range.start_x;
                    E.cy = range.start_y;
                } else {
                    E.cx = range.end_x;
                    E.cy = range.end_y;
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

        case SHIFT_ALT_UP:
        case SHIFT_ALT_DOWN:
            E.is_selected = 0;
            editorInsertRow(E.cy, E.row[E.cy].data, E.row[E.cy].size);
            if (c == SHIFT_ALT_DOWN)
                E.cy++;
            break;

        // Action: Move Line Up
        // Action: Move Line Down
        case ALT_UP:
        case ALT_DOWN: {
            EditorSelectRange range;
            getSelectStartEnd(&range);
            if (c == ALT_UP) {
                if (range.start_y == 0)
                    break;
            } else {
                if (range.end_y == E.num_rows - 1)
                    break;
            }

            EditorAction* action = malloc(sizeof(EditorAction));
            if (!action)
                PANIC("malloc");

            int old_cx = E.cx;
            int old_cy = E.cy;
            int old_select_x = E.select_x;
            int old_select_y = E.select_y;

            range.start_x = 0;
            int paste_x = 0;
            if (c == ALT_UP) {
                range.start_y--;
                range.end_x = E.row[range.end_y].size;
                editorCopyText(&action->added_text, range);
                //  Move empty string at the start to the end
                char* temp = action->added_text.data[0];
                memmove(&action->added_text.data[0],
                        &action->added_text.data[1],
                        (action->added_text.size - 1) * sizeof(char*));
                action->added_text.data[action->added_text.size - 1] = temp;
            } else {
                range.end_x = 0;
                range.end_y++;
                editorCopyText(&action->added_text, range);
                // Move empty string at the end to the start
                char* temp =
                    action->added_text.data[action->added_text.size - 1];
                memmove(&action->added_text.data[1],
                        &action->added_text.data[0],
                        (action->added_text.size - 1) * sizeof(char*));
                action->added_text.data[0] = temp;
            }
            action->deleted_range = range;
            editorCopyText(&action->deleted_text, range);
            editorDeleteText(range);

            if (c == ALT_UP) {
                old_cy--;
                old_select_y--;
            } else {
                paste_x = E.row[E.cy].size;
                old_cy++;
                old_select_y++;
            }

            range.start_x = paste_x;
            range.start_y = E.cy;
            editorPasteText(&action->added_text, paste_x, E.cy);
            range.end_x = E.cx;
            range.end_y = E.cy;
            action->added_range = range;

            E.cx = old_cx;
            E.cy = old_cy;
            E.select_x = old_select_x;
            E.select_y = old_select_y;

            editorAppendAction(action);
        } break;

        case CTRL_KEY('l'):
        case ESC:
            break;

        // Mouse input
        case MOUSE_PRESSED:
            if (!isValidMousePos(x, y))
                break;
            pressed = 1;
            prev_x = x;
            prev_y = y;

            E.is_selected = 0;
            E.bracket_autocomplete = 0;

            if (!mousePosToEditorPos(&x, &y))
                break;

            E.cy = y;
            E.cx = editorRowRxToCx(&E.row[y], x);
            E.sx = x;
            break;

        case MOUSE_RELEASED:
        case MOUSE_MOVE:
            if (!pressed)
                break;

            if (c == MOUSE_RELEASED) {
                pressed = 0;
                if (x == prev_x && y == prev_y)
                    break;
            }

            if (!isValidMousePos(x, y) || !mousePosToEditorPos(&x, &y))
                break;

            E.is_selected = 1;
            E.cx = editorRowRxToCx(&E.row[y], x);
            E.cy = y;
            E.sx = x;
            break;

        case WHEEL_UP:
            should_scroll = 0;
            if (E.row_offset - 3 > 0)
                E.row_offset -= 3;
            else
                E.row_offset = 0;
            break;

        case WHEEL_DOWN:
            should_scroll = 0;
            if (E.row_offset + E.rows + 3 < E.num_rows)
                E.row_offset += 3;
            else if (E.num_rows - E.rows < 0)
                E.row_offset = 0;
            else
                E.row_offset = E.num_rows - E.rows;
            break;

        case SCROLL_PRESSED:
        case SCROLL_RELEASED:
            break;

        // Action: Input
        default: {
            if (!isprint(c) && c != '\t')
                break;

            EditorAction* action = malloc(sizeof(EditorAction));
            if (!action)
                PANIC("malloc");
            getSelectStartEnd(&action->deleted_range);

            if (E.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.is_selected = 0;
            } else {
                action->deleted_text.size = 0;
                action->deleted_text.data = NULL;
            }

            int x_offset = 0;
            action->added_range.start_x = E.cx;
            action->added_range.start_y = E.cy;

            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            if (close_bracket) {
                editorInsertChar(c);
                editorInsertChar(close_bracket);
                x_offset = 1;
                E.cx--;
                E.bracket_autocomplete++;
            } else if (open_bracket) {
                if (E.bracket_autocomplete && E.row[E.cy].data[E.cx] == c) {
                    E.bracket_autocomplete--;
                    x_offset = -1;
                    E.cx++;
                } else {
                    editorInsertChar(c);
                }
            } else if (c == '\'' || c == '"') {
                if (E.row[E.cy].data[E.cx] != c) {
                    editorInsertChar(c);
                    editorInsertChar(c);
                    x_offset = 1;
                    E.cx--;
                    E.bracket_autocomplete++;
                } else if (E.bracket_autocomplete &&
                           E.row[E.cy].data[E.cx] == c) {
                    E.bracket_autocomplete--;
                    x_offset = -1;
                    E.cx++;
                } else {
                    editorInsertChar(c);
                }
            } else {
                editorInsertChar(c);
            }

            action->added_range.end_x = E.cx + x_offset;
            action->added_range.end_y = E.cy;
            editorCopyText(&action->added_text, action->added_range);

            if (x_offset != -1) {
                editorAppendAction(action);
            } else {
                editorFreeAction(action);
            }

            E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
            E.is_selected = 0;
        } break;
    }

    if (!E.is_selected) {
        E.select_x = E.cx;
        E.select_y = E.cy;
    }

    if (should_scroll)
        editorScroll();
    quit_protect = 1;
}
