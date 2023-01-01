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
    char* buf = malloc_s(bufsize);

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
                        buf = realloc_s(buf, bufsize);
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
    EditorRow* row = (E.cursor.y >= E.num_rows) ? NULL : &(E.row[E.cursor.y]);
    switch (key) {
        case ARROW_LEFT:
            if (E.cursor.x != 0) {
                E.cursor.x--;
                E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
            } else if (E.cursor.y > 0) {
                E.cursor.y--;
                E.cursor.x = E.row[E.cursor.y].size;
                E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cursor.x < row->size) {
                E.cursor.x++;
                E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
            } else if (row && (E.cursor.y + 1 < E.num_rows) &&
                       E.cursor.x == row->size) {
                E.cursor.y++;
                E.cursor.x = 0;
                E.sx = 0;
            }

            break;
        case ARROW_UP:
            if (E.cursor.y != 0) {
                E.cursor.y--;
                E.cursor.x = editorRowSxToCx(&(E.row[E.cursor.y]), E.sx);
            }
            break;
        case ARROW_DOWN:
            if (E.cursor.y + 1 < E.num_rows) {
                E.cursor.y++;
                E.cursor.x = editorRowSxToCx(&(E.row[E.cursor.y]), E.sx);
            }
            break;
    }
    row = (E.cursor.y >= E.num_rows) ? NULL : &(E.row[E.cursor.y]);
    int row_len = row ? row->size : 0;
    if (E.cursor.x > row_len) {
        E.cursor.x = row_len;
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

    int should_record_action = 0;
    EditorAction* action = calloc_s(1, sizeof(EditorAction));
    action->old_cursor = E.cursor;

    switch (c) {
        // Action: Newline
        case '\r': {
            should_record_action = 1;

            getSelectStartEnd(&action->deleted_range);

            if (E.cursor.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.cursor.is_selected = 0;
            }

            E.bracket_autocomplete = 0;

            action->added_range.start_x = E.cursor.x;
            action->added_range.start_y = E.cursor.y;
            editorInsertNewline();
            action->added_range.end_x = E.cursor.x;
            action->added_range.end_y = E.cursor.y;
            editorCopyText(&action->added_text, action->added_range);
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
            if (E.cursor.x == 0)
                break;
            E.cursor.x = 0;
            E.sx = 0;
            E.cursor.is_selected = (c == SHIFT_HOME || c == SHIFT_CTRL_LEFT);
            E.bracket_autocomplete = 0;
            break;

        case END_KEY:
        case CTRL_RIGHT:
        case SHIFT_END:
        case SHIFT_CTRL_RIGHT:
            if (E.cursor.y < E.num_rows &&
                E.cursor.x != E.row[E.cursor.y].size) {
                E.cursor.x = E.row[E.cursor.y].size;
                E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
                E.cursor.is_selected =
                    (c == SHIFT_END || c == SHIFT_CTRL_RIGHT);
                E.bracket_autocomplete = 0;
            }
            break;

        case CTRL_KEY('f'):
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorFind();
            break;

        case CTRL_KEY('g'):
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorGotoLine();
            break;

        case CTRL_KEY('p'):
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorSetting();
            break;

        case CTRL_KEY('a'):
            if (E.num_rows == 1 && E.row[0].size == 0)
                break;
            E.cursor.is_selected = 1;
            E.bracket_autocomplete = 0;
            E.cursor.y = E.num_rows - 1;
            E.cursor.x = E.row[E.num_rows - 1].size;
            E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
            E.cursor.select_y = 0;
            E.cursor.select_x = 0;
            break;

        // Action: Delete
        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE: {
            if (!E.cursor.is_selected) {
                if (c == DEL_KEY) {
                    if (E.cursor.y == E.num_rows - 1 &&
                        E.cursor.x == E.row[E.num_rows - 1].size)
                        break;
                } else if (E.cursor.x == 0 && E.cursor.y == 0) {
                    break;
                }
            }

            should_record_action = 1;

            if (E.cursor.is_selected) {
                getSelectStartEnd(&action->deleted_range);
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.cursor.is_selected = 0;
                break;
            }

            if (c == DEL_KEY)
                editorMoveCursor(ARROW_RIGHT);
            else if (E.bracket_autocomplete &&
                     (isCloseBracket(E.row[E.cursor.y].data[E.cursor.x]) ==
                          E.row[E.cursor.y].data[E.cursor.x - 1] ||
                      (E.row[E.cursor.y].data[E.cursor.x] == '\'' &&
                       E.row[E.cursor.y].data[E.cursor.x - 1] == '\'') ||
                      (E.row[E.cursor.y].data[E.cursor.x] == '"' &&
                       E.row[E.cursor.y].data[E.cursor.x - 1] == '"'))) {
                editorMoveCursor(ARROW_RIGHT);
            }

            action->deleted_range.end_x = E.cursor.x;
            action->deleted_range.end_y = E.cursor.y;

            if (E.bracket_autocomplete) {
                E.bracket_autocomplete--;
                editorMoveCursor(ARROW_LEFT);
            }
            char deleted_char = E.row[E.cursor.y].data[E.cursor.x - 1];
            editorMoveCursor(ARROW_LEFT);
            if (deleted_char == ' ') {
                int should_delete_tab = 1;
                for (int i = 0; i < E.cursor.x; i++) {
                    if (!isspace(E.row[E.cursor.y].data[i])) {
                        should_delete_tab = 0;
                    }
                }
                if (should_delete_tab) {
                    int idx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
                    while (idx % E.cfg->tab_size != 0) {
                        editorMoveCursor(ARROW_LEFT);
                        idx--;
                    }
                }
            }
            action->deleted_range.start_x = E.cursor.x;
            action->deleted_range.start_y = E.cursor.y;
            editorCopyText(&action->deleted_text, action->deleted_range);
            editorDeleteText(action->deleted_range);
        } break;

        // Action: Cut
        case CTRL_KEY('x'): {
            if (!E.cursor.is_selected)
                break;

            should_record_action = 1;

            getSelectStartEnd(&action->deleted_range);
            editorCopyText(&action->deleted_text, action->deleted_range);
            editorFreeClipboardContent(&E.clipboard);
            editorCopyText(&E.clipboard, action->deleted_range);
            editorDeleteText(action->deleted_range);
            E.cursor.is_selected = 0;
        } break;

        // Copy
        case CTRL_KEY('c'): {
            if (!E.cursor.is_selected)
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

            should_record_action = 1;

            getSelectStartEnd(&action->deleted_range);

            if (E.cursor.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.cursor.is_selected = 0;
            }

            action->added_range.start_x = E.cursor.x;
            action->added_range.start_y = E.cursor.y;
            editorPasteText(&E.clipboard, E.cursor.x, E.cursor.y);

            action->added_range.end_x = E.cursor.x;
            action->added_range.end_y = E.cursor.y;
            editorCopyText(&action->added_text, action->added_range);
        } break;

        // Undo
        case CTRL_KEY('z'):
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorUndo();
            break;

        // Redo
        case CTRL_KEY('y'):
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            editorRedo();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            {
                if (c == PAGE_UP) {
                    E.cursor.y = E.row_offset;
                } else if (c == PAGE_DOWN) {
                    E.cursor.y = E.row_offset + E.rows - 1;
                    if (E.cursor.y >= E.num_rows)
                        E.cursor.y = E.num_rows - 1;
                }
                int times = E.rows;
                while (times--) {
                    if (c == PAGE_UP) {
                        if (E.cursor.y == 0) {
                            E.cursor.x = 0;
                            E.sx = 0;
                            break;
                        }
                        editorMoveCursor(ARROW_UP);
                    } else {
                        if (E.cursor.y == E.num_rows - 1) {
                            E.cursor.x = E.row[E.cursor.y].size;
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
            if (E.cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&range);

                if (c == ARROW_UP || c == ARROW_LEFT) {
                    E.cursor.x = range.start_x;
                    E.cursor.y = range.start_y;
                } else {
                    E.cursor.x = range.end_x;
                    E.cursor.y = range.end_y;
                }
                E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
                if (c == ARROW_UP || c == ARROW_DOWN) {
                    editorMoveCursor(c);
                }
                E.cursor.is_selected = 0;
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
            E.cursor.is_selected = 1;
            E.bracket_autocomplete = 0;
            editorMoveCursor(c - 9);
            break;

        case CTRL_HOME:
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            E.cursor.y = 0;
            E.cursor.x = 0;
            E.sx = 0;
            break;

        case CTRL_END:
            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;
            E.cursor.y = E.num_rows - 1;
            E.cursor.x = E.row[E.num_rows - 1].size;
            E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
            break;

        case SHIFT_ALT_UP:
        case SHIFT_ALT_DOWN:
            E.cursor.is_selected = 0;
            editorInsertRow(E.cursor.y, E.row[E.cursor.y].data,
                            E.row[E.cursor.y].size);
            if (c == SHIFT_ALT_DOWN)
                E.cursor.y++;
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

            should_record_action = 1;

            int old_cx = E.cursor.x;
            int old_cy = E.cursor.y;
            int old_select_x = E.cursor.select_x;
            int old_select_y = E.cursor.select_y;

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
                paste_x = E.row[E.cursor.y].size;
                old_cy++;
                old_select_y++;
            }

            range.start_x = paste_x;
            range.start_y = E.cursor.y;
            editorPasteText(&action->added_text, paste_x, E.cursor.y);
            range.end_x = E.cursor.x;
            range.end_y = E.cursor.y;
            action->added_range = range;

            E.cursor.x = old_cx;
            E.cursor.y = old_cy;
            E.cursor.select_x = old_select_x;
            E.cursor.select_y = old_select_y;
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

            E.cursor.is_selected = 0;
            E.bracket_autocomplete = 0;

            if (!mousePosToEditorPos(&x, &y))
                break;

            E.cursor.y = y;
            E.cursor.x = editorRowRxToCx(&E.row[y], x);
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

            E.cursor.is_selected = 1;
            E.cursor.x = editorRowRxToCx(&E.row[y], x);
            E.cursor.y = y;
            E.sx = x;
            break;

        // Scroll up
        case WHEEL_UP:
        case CTRL_UP: {
            int scroll_dist = (c == WHEEL_UP) ? 3 : 1;
            should_scroll = 0;
            if (E.row_offset - scroll_dist > 0)
                E.row_offset -= scroll_dist;
            else
                E.row_offset = 0;
        } break;

        // Scroll down
        case WHEEL_DOWN:
        case CTRL_DOWN: {
            int scroll_dist = (c == WHEEL_DOWN) ? 3 : 1;
            should_scroll = 0;
            if (E.row_offset + E.rows + scroll_dist < E.num_rows)
                E.row_offset += scroll_dist;
            else if (E.num_rows - E.rows < 0)
                E.row_offset = 0;
            else
                E.row_offset = E.num_rows - E.rows;
        } break;

        case SCROLL_PRESSED:
        case SCROLL_RELEASED:
            break;

        // Action: Input
        default: {
            if (!isprint(c) && c != '\t')
                break;

            should_record_action = 1;

            getSelectStartEnd(&action->deleted_range);

            if (E.cursor.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                E.cursor.is_selected = 0;
            }

            int x_offset = 0;
            action->added_range.start_x = E.cursor.x;
            action->added_range.start_y = E.cursor.y;

            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            if (close_bracket) {
                editorInsertChar(c);
                editorInsertChar(close_bracket);
                x_offset = 1;
                E.cursor.x--;
                E.bracket_autocomplete++;
            } else if (open_bracket) {
                if (E.bracket_autocomplete &&
                    E.row[E.cursor.y].data[E.cursor.x] == c) {
                    E.bracket_autocomplete--;
                    x_offset = -1;
                    E.cursor.x++;
                } else {
                    editorInsertChar(c);
                }
            } else if (c == '\'' || c == '"') {
                if (E.row[E.cursor.y].data[E.cursor.x] != c) {
                    editorInsertChar(c);
                    editorInsertChar(c);
                    x_offset = 1;
                    E.cursor.x--;
                    E.bracket_autocomplete++;
                } else if (E.bracket_autocomplete &&
                           E.row[E.cursor.y].data[E.cursor.x] == c) {
                    E.bracket_autocomplete--;
                    x_offset = -1;
                    E.cursor.x++;
                } else {
                    editorInsertChar(c);
                }
            } else {
                editorInsertChar(c);
            }

            action->added_range.end_x = E.cursor.x + x_offset;
            action->added_range.end_y = E.cursor.y;
            editorCopyText(&action->added_text, action->added_range);

            E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
            E.cursor.is_selected = 0;

            if (x_offset == -1) {
                should_record_action = 0;
            }
        } break;
    }

    if (!E.cursor.is_selected) {
        E.cursor.select_x = E.cursor.x;
        E.cursor.select_y = E.cursor.y;
    }

    if (should_record_action) {
        action->new_cursor = E.cursor;
        editorAppendAction(action);
    } else {
        editorFreeAction(action);
    }

    if (should_scroll)
        editorScroll();
    quit_protect = 1;
}
