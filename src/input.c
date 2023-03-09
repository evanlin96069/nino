#include "input.h"

#include <ctype.h>
#include <stdbool.h>
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

static int getMousePosField(int x, int y) {
    if (y < 0 || y >= gEditor.screen_rows)
        return FIELD_ERROR;
    if (y == 0)
        return FIELD_TOP_STATUS;
    if (y == gEditor.screen_rows - 1)
        return FIELD_PROMPT;
    if (y == gEditor.screen_rows - 2)
        return FIELD_STATUS;
    if (x < gCurFile->num_rows_digits + 1)
        return FIELD_LINE_NUMBER;
    return FIELD_TEXT;
}

static bool mousePosToEditorPos(int* x, int* y) {
    int row = gCurFile->row_offset + *y - 1;
    if (row >= gCurFile->num_rows)
        return false;
    int col = *x - gCurFile->num_rows_digits - 1 + gCurFile->col_offset;
    if (col > gCurFile->row[row].rsize)
        col = gCurFile->row[row].rsize;
    *x = col;
    *y = row;
    return true;
}

static void scrollUp(int dist) {
    if (gCurFile->row_offset - dist > 0)
        gCurFile->row_offset -= dist;
    else
        gCurFile->row_offset = 0;
}

static void scrollDown(int dist) {
    if (gCurFile->row_offset + gEditor.display_rows + dist < gCurFile->num_rows)
        gCurFile->row_offset += dist;
    else if (gCurFile->num_rows - gEditor.display_rows < 0)
        gCurFile->row_offset = 0;
    else
        gCurFile->row_offset = gCurFile->num_rows - gEditor.display_rows;
}

char* editorPrompt(char* prompt, int state, void (*callback)(char*, int)) {
    int prev_state = gEditor.state;
    gEditor.state = state;

    size_t bufsize = 128;
    char* buf = malloc_s(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    int start = 0;
    while (prompt[start] != '\0' && prompt[start] != '%') {
        start++;
    }
    gEditor.px = start;
    while (true) {
        editorSetStatusMsg(prompt, buf);
        editorRefreshScreen();
        int x, y;
        int c = editorReadKey(&x, &y);
        size_t idx = gEditor.px - start;
        switch (c) {
            case DEL_KEY:
                if (idx != buflen)
                    idx++;
                else
                    break;
                // fall through
            case CTRL_KEY('h'):
            case BACKSPACE:
                if (idx != 0) {
                    memmove(&buf[idx - 1], &buf[idx], buflen - idx + 1);
                    buflen--;
                    idx--;
                    if (callback)
                        callback(buf, c);
                }
                break;

            case HOME_KEY:
                idx = 0;
                break;

            case END_KEY:
                idx = buflen;
                break;

            case ARROW_LEFT:
                if (idx != 0)
                    idx--;
                break;

            case ARROW_RIGHT:
                if (idx < buflen)
                    idx++;
                break;

            case WHEEL_UP:
                scrollUp(3);
                break;

            case WHEEL_DOWN:
                scrollDown(3);
                break;

            case MOUSE_PRESSED: {
                int field = getMousePosField(x, y);
                if (field == FIELD_PROMPT) {
                    if (x >= start) {
                        size_t cx = x - start;
                        if (cx < buflen)
                            idx = cx;
                        else
                            idx = buflen;
                    }
                    break;
                }

                if (field == FIELD_TEXT && mousePosToEditorPos(&x, &y)) {
                    gCurFile->cursor.y = y;
                    gCurFile->cursor.x = editorRowRxToCx(&gCurFile->row[y], x);
                    gCurFile->sx = x;
                }
            }
                // fall through
            case CTRL_KEY('q'):
            case ESC:
                editorSetStatusMsg("");
                gEditor.state = prev_state;
                if (callback)
                    callback(buf, c);
                free(buf);
                return NULL;

            case '\r':
                if (buflen != 0) {
                    editorSetStatusMsg("");
                    gEditor.state = prev_state;
                    if (callback)
                        callback(buf, c);
                    return buf;
                }
                break;

            default:
                if (isprint(c)) {
                    if (buflen == bufsize - 1) {
                        bufsize *= 2;
                        buf = realloc_s(buf, bufsize);
                    }
                    buflen++;
                    memmove(&buf[idx + 1], &buf[idx], buflen - idx);
                    buf[idx] = c;
                    idx++;
                }

                if (callback)
                    callback(buf, c);
                break;
        }
        gEditor.px = start + idx;
    }
}

void editorMoveCursor(int key) {
    EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    switch (key) {
        case ARROW_LEFT:
            if (gCurFile->cursor.x != 0) {
                gCurFile->cursor.x = editorRowPreviousUTF8(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
                gCurFile->sx = editorRowCxToRx(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
            } else if (gCurFile->cursor.y > 0) {
                gCurFile->cursor.y--;
                gCurFile->cursor.x = gCurFile->row[gCurFile->cursor.y].size;
                gCurFile->sx = editorRowCxToRx(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
            }
            break;

        case ARROW_RIGHT:
            if (row && gCurFile->cursor.x < row->size) {
                gCurFile->cursor.x = editorRowNextUTF8(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
                gCurFile->sx = editorRowCxToRx(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
            } else if (row && (gCurFile->cursor.y + 1 < gCurFile->num_rows) &&
                       gCurFile->cursor.x == row->size) {
                gCurFile->cursor.y++;
                gCurFile->cursor.x = 0;
                gCurFile->sx = 0;
            }
            break;

        case ARROW_UP:
            if (gCurFile->cursor.y != 0) {
                gCurFile->cursor.y--;
                gCurFile->cursor.x = editorRowRxToCx(
                    &(gCurFile->row[gCurFile->cursor.y]), gCurFile->sx);
            }
            break;

        case ARROW_DOWN:
            if (gCurFile->cursor.y + 1 < gCurFile->num_rows) {
                gCurFile->cursor.y++;
                gCurFile->cursor.x = editorRowRxToCx(
                    &(gCurFile->row[gCurFile->cursor.y]), gCurFile->sx);
            }
            break;
    }
    row = (gCurFile->cursor.y >= gCurFile->num_rows)
              ? NULL
              : &(gCurFile->row[gCurFile->cursor.y]);
    int row_len = row ? row->size : 0;
    if (gCurFile->cursor.x > row_len) {
        gCurFile->cursor.x = row_len;
    }
}

static void editorMoveCursorWordLeft() {
    if (gCurFile->cursor.x == 0) {
        if (gCurFile->cursor.y == 0)
            return;
        editorMoveCursor(ARROW_LEFT);
    }

    EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    while (gCurFile->cursor.x > 0 &&
           isSeparator(row->data[gCurFile->cursor.x - 1])) {
        gCurFile->cursor.x--;
    }
    while (gCurFile->cursor.x > 0 &&
           !isSeparator(row->data[gCurFile->cursor.x - 1])) {
        gCurFile->cursor.x--;
    }
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}

static void editorMoveCursorWordRight() {
    if (gCurFile->cursor.x == gCurFile->row[gCurFile->cursor.y].size) {
        if (gCurFile->cursor.y == gCurFile->num_rows - 1)
            return;
        gCurFile->cursor.x = 0;
        gCurFile->cursor.y++;
    }

    EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    while (gCurFile->cursor.x < row->size &&
           isSeparator(row->data[gCurFile->cursor.x])) {
        gCurFile->cursor.x++;
    }
    while (gCurFile->cursor.x < row->size &&
           !isSeparator(row->data[gCurFile->cursor.x])) {
        gCurFile->cursor.x++;
    }
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
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

static int getClickedFile(int x) {
    if (gEditor.loading)
        return -1;

    int len = 0;
    for (int i = 0; i < gEditor.file_count; i++) {
        if (len >= gEditor.screen_cols)
            break;

        const EditorFile* file = &gEditor.files[i];
        len += strlen(file->filename ? file->filename : "Untitled") + 2;
        if (file->dirty)
            len++;
        if (len > x)
            return i;
    }
    return -1;
}

bool moveMouse(int x, int y) {
    if (getMousePosField(x, y) != FIELD_TEXT || !mousePosToEditorPos(&x, &y))
        return false;
    gCurFile->cursor.is_selected = true;
    gCurFile->cursor.x = editorRowRxToCx(&gCurFile->row[y], x);
    gCurFile->cursor.y = y;
    gCurFile->sx = x;
    return true;
}

void editorProcessKeypress() {
    // Protect closing file with unsaved changes
    static bool close_protect = true;
    // Protect quiting program with unsaved files
    static bool quit_protect = true;
    static bool pressed = false;
    static int curr_x = 0;
    static int curr_y = 0;

    int x, y;
    int c = editorReadKey(&x, &y);

    bool should_scroll = true;

    editorSetStatusMsg("");

    bool should_record_action = false;
    EditorAction* action = calloc_s(1, sizeof(EditorAction));
    action->old_cursor = gCurFile->cursor;

    switch (c) {
        // Action: Newline
        case '\r': {
            should_record_action = true;

            getSelectStartEnd(&action->deleted_range);

            if (gCurFile->cursor.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                gCurFile->cursor.is_selected = false;
            }

            gCurFile->bracket_autocomplete = 0;

            action->added_range.start_x = gCurFile->cursor.x;
            action->added_range.start_y = gCurFile->cursor.y;
            editorInsertNewline();
            action->added_range.end_x = gCurFile->cursor.x;
            action->added_range.end_y = gCurFile->cursor.y;
            editorCopyText(&action->added_text, action->added_range);
        } break;

        // Quit editor
        case CTRL_KEY('q'): {
            close_protect = true;
            editorFreeAction(action);
            bool dirty = false;
            for (int i = 0; i < gEditor.file_count; i++) {
                if (gEditor.files[i].dirty) {
                    dirty = true;
                    break;
                }
            }
            if (dirty && quit_protect) {
                editorSetStatusMsg(
                    "Files have unsaved changes. Press ^Q again to quit "
                    "anyway.");
                quit_protect = false;
                return;
            }
            editorFree();
            exit(EXIT_SUCCESS);
        }

        // Close current file
        case CTRL_KEY('w'):
            quit_protect = true;
            editorFreeAction(action);
            if (gCurFile->dirty && close_protect) {
                editorSetStatusMsg(
                    "File has unsaved changes. Press ^W again to close file "
                    "anyway.");
                close_protect = false;
                return;
            }
            editorRemoveFile(gEditor.file_index);
            if (gEditor.file_index == gEditor.file_count)
                editorChangeToFile(gEditor.file_index - 1);
            return;

        // Save
        case CTRL_KEY('s'):
            should_scroll = false;
            if (gCurFile->dirty)
                editorSave(gCurFile, 0);
            break;

        // Save all
        case ALT_KEY(CTRL_KEY('s')):
            // Alt+Ctrl+S
            should_scroll = false;
            for (int i = 0; i < gEditor.file_count; i++) {
                if (gEditor.files[i].dirty) {
                    editorSave(&gEditor.files[i], 0);
                }
            }
            break;

        // Save as
        case CTRL_KEY('o'):
            should_scroll = false;
            editorSave(gCurFile, 1);
            break;

        case HOME_KEY:
        case SHIFT_HOME:
            if (gCurFile->cursor.x == 0)
                break;
            gCurFile->cursor.x = 0;
            gCurFile->sx = 0;
            gCurFile->cursor.is_selected = c == (SHIFT_HOME);
            gCurFile->bracket_autocomplete = 0;
            break;

        case END_KEY:
        case SHIFT_END:
            if (gCurFile->cursor.y < gCurFile->num_rows &&
                gCurFile->cursor.x != gCurFile->row[gCurFile->cursor.y].size) {
                gCurFile->cursor.x = gCurFile->row[gCurFile->cursor.y].size;
                gCurFile->sx = editorRowCxToRx(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
                gCurFile->cursor.is_selected = (c == SHIFT_END);
                gCurFile->bracket_autocomplete = 0;
            }
            break;

        case CTRL_LEFT:
        case SHIFT_CTRL_LEFT:
            editorMoveCursorWordLeft();
            gCurFile->cursor.is_selected = (c == SHIFT_CTRL_LEFT);
            gCurFile->bracket_autocomplete = 0;
            break;

        case CTRL_RIGHT:
        case SHIFT_CTRL_RIGHT:
            editorMoveCursorWordRight();
            gCurFile->cursor.is_selected = (c == SHIFT_CTRL_RIGHT);
            gCurFile->bracket_autocomplete = 0;
            break;

        // Find
        case CTRL_KEY('f'):
            should_scroll = false;
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            editorFind();
            break;

        // Goto line
        case CTRL_KEY('g'):
            should_scroll = false;
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            editorGotoLine();
            break;

        // Prompt
        case CTRL_KEY('p'):
            should_scroll = false;
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            editorSetting();
            break;

        case CTRL_KEY('a'):
            if (gCurFile->num_rows == 1 && gCurFile->row[0].size == 0)
                break;
            gCurFile->cursor.is_selected = true;
            gCurFile->bracket_autocomplete = 0;
            gCurFile->cursor.y = gCurFile->num_rows - 1;
            gCurFile->cursor.x = gCurFile->row[gCurFile->num_rows - 1].size;
            gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                           gCurFile->cursor.x);
            gCurFile->cursor.select_y = 0;
            gCurFile->cursor.select_x = 0;
            break;

        // Action: Delete
        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE: {
            if (!gCurFile->cursor.is_selected) {
                if (c == DEL_KEY) {
                    if (gCurFile->cursor.y == gCurFile->num_rows - 1 &&
                        gCurFile->cursor.x ==
                            gCurFile->row[gCurFile->num_rows - 1].size)
                        break;
                } else if (gCurFile->cursor.x == 0 && gCurFile->cursor.y == 0) {
                    break;
                }
            }

            should_record_action = true;

            if (gCurFile->cursor.is_selected) {
                getSelectStartEnd(&action->deleted_range);
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                gCurFile->cursor.is_selected = false;
                break;
            }

            bool should_delete_bracket =
                gCurFile->bracket_autocomplete &&
                (isCloseBracket(gCurFile->row[gCurFile->cursor.y]
                                    .data[gCurFile->cursor.x]) ==
                     gCurFile->row[gCurFile->cursor.y]
                         .data[gCurFile->cursor.x - 1] ||
                 (gCurFile->row[gCurFile->cursor.y].data[gCurFile->cursor.x] ==
                      '\'' &&
                  gCurFile->row[gCurFile->cursor.y]
                          .data[gCurFile->cursor.x - 1] == '\'') ||
                 (gCurFile->row[gCurFile->cursor.y].data[gCurFile->cursor.x] ==
                      '"' &&
                  gCurFile->row[gCurFile->cursor.y]
                          .data[gCurFile->cursor.x - 1] == '"'));

            if (c == DEL_KEY)
                editorMoveCursor(ARROW_RIGHT);
            else if (should_delete_bracket) {
                gCurFile->bracket_autocomplete--;
                editorMoveCursor(ARROW_RIGHT);
            }

            action->deleted_range.end_x = gCurFile->cursor.x;
            action->deleted_range.end_y = gCurFile->cursor.y;

            if (should_delete_bracket)
                editorMoveCursor(ARROW_LEFT);

            char deleted_char =
                gCurFile->row[gCurFile->cursor.y].data[gCurFile->cursor.x - 1];
            editorMoveCursor(ARROW_LEFT);
            if (CONVAR_GETINT(backspace) && deleted_char == ' ') {
                bool should_delete_tab = true;
                for (int i = 0; i < gCurFile->cursor.x; i++) {
                    if (!isspace(gCurFile->row[gCurFile->cursor.y].data[i])) {
                        should_delete_tab = false;
                    }
                }
                if (should_delete_tab) {
                    int idx = editorRowCxToRx(
                        &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
                    while (idx % CONVAR_GETINT(tabsize) != 0) {
                        editorMoveCursor(ARROW_LEFT);
                        idx--;
                    }
                }
            }
            action->deleted_range.start_x = gCurFile->cursor.x;
            action->deleted_range.start_y = gCurFile->cursor.y;
            editorCopyText(&action->deleted_text, action->deleted_range);
            editorDeleteText(action->deleted_range);
        } break;

        // Action: Cut
        case CTRL_KEY('x'): {
            if (!gCurFile->cursor.is_selected) {
                should_scroll = false;
                break;
            }

            should_record_action = true;

            getSelectStartEnd(&action->deleted_range);
            editorCopyText(&action->deleted_text, action->deleted_range);
            editorFreeClipboardContent(&gEditor.clipboard);
            editorCopyText(&gEditor.clipboard, action->deleted_range);
            editorDeleteText(action->deleted_range);
            gCurFile->cursor.is_selected = false;
        } break;

        // Copy
        case CTRL_KEY('c'): {
            editorFreeClipboardContent(&gEditor.clipboard);
            should_scroll = false;
            if (gCurFile->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&range);
                editorCopyText(&gEditor.clipboard, range);
            } else {
                // Copy line
                EditorSelectRange range = {
                    0, gCurFile->cursor.y,
                    gCurFile->row[gCurFile->cursor.y].size, gCurFile->cursor.y};
                editorCopyText(&gEditor.clipboard, range);
            }
        } break;

        // Action: Paste
        case CTRL_KEY('v'): {
            if (!gEditor.clipboard.size)
                break;

            should_record_action = true;

            getSelectStartEnd(&action->deleted_range);

            if (gCurFile->cursor.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                gCurFile->cursor.is_selected = false;
            }

            action->added_range.start_x = gCurFile->cursor.x;
            action->added_range.start_y = gCurFile->cursor.y;
            editorPasteText(&gEditor.clipboard, gCurFile->cursor.x,
                            gCurFile->cursor.y);

            action->added_range.end_x = gCurFile->cursor.x;
            action->added_range.end_y = gCurFile->cursor.y;
            editorCopyText(&action->added_text, action->added_range);
        } break;

        // Undo
        case CTRL_KEY('z'):
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            should_scroll = editorUndo();
            break;

        // Redo
        case CTRL_KEY('y'):
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            should_scroll = editorRedo();
            break;

        // Previous file
        case CTRL_KEY('['):
            should_scroll = false;
            if (gEditor.file_count < 2)
                break;

            if (gEditor.file_index == 0)
                editorChangeToFile(gEditor.file_count - 1);
            else
                editorChangeToFile(gEditor.file_index - 1);
            break;

        // Next file
        case CTRL_KEY(']'):
            should_scroll = false;
            if (gEditor.file_count < 2)
                break;

            if (gEditor.file_index == gEditor.file_count - 1)
                editorChangeToFile(0);
            else
                editorChangeToFile(gEditor.file_index + 1);
            break;

        case SHIFT_PAGE_UP:
        case SHIFT_PAGE_DOWN:
        case PAGE_UP:
        case PAGE_DOWN:
            gCurFile->cursor.is_selected =
                (c == SHIFT_PAGE_UP || c == SHIFT_PAGE_DOWN);
            gCurFile->bracket_autocomplete = 0;
            {
                if (c == PAGE_UP || c == SHIFT_PAGE_UP) {
                    gCurFile->cursor.y = gCurFile->row_offset;
                } else if (c == PAGE_DOWN || c == SHIFT_PAGE_DOWN) {
                    gCurFile->cursor.y =
                        gCurFile->row_offset + gEditor.display_rows - 1;
                    if (gCurFile->cursor.y >= gCurFile->num_rows)
                        gCurFile->cursor.y = gCurFile->num_rows - 1;
                }
                int times = gEditor.display_rows;
                while (times--) {
                    if (c == PAGE_UP || c == SHIFT_PAGE_UP) {
                        if (gCurFile->cursor.y == 0) {
                            gCurFile->cursor.x = 0;
                            gCurFile->sx = 0;
                            break;
                        }
                        editorMoveCursor(ARROW_UP);
                    } else {
                        if (gCurFile->cursor.y == gCurFile->num_rows - 1) {
                            gCurFile->cursor.x =
                                gCurFile->row[gCurFile->cursor.y].size;
                            break;
                        }
                        editorMoveCursor(ARROW_DOWN);
                    }
                }
            }
            break;

        case SHIFT_CTRL_PAGE_UP:
        case CTRL_PAGE_UP:
            gCurFile->cursor.is_selected = (c == SHIFT_CTRL_PAGE_UP);
            gCurFile->bracket_autocomplete = 0;
            while (gCurFile->cursor.y > 0) {
                editorMoveCursor(ARROW_UP);
                if (gCurFile->row[gCurFile->cursor.y].data[0] == '\0') {
                    break;
                }
            }
            break;

        case SHIFT_CTRL_PAGE_DOWN:
        case CTRL_PAGE_DOWN:
            gCurFile->cursor.is_selected = (c == SHIFT_CTRL_PAGE_DOWN);
            gCurFile->bracket_autocomplete = 0;
            while (gCurFile->cursor.y < gCurFile->num_rows - 1) {
                editorMoveCursor(ARROW_DOWN);
                if (gCurFile->row[gCurFile->cursor.y].data[0] == '\0') {
                    break;
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            if (gCurFile->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&range);

                if (c == ARROW_UP || c == ARROW_LEFT) {
                    gCurFile->cursor.x = range.start_x;
                    gCurFile->cursor.y = range.start_y;
                } else {
                    gCurFile->cursor.x = range.end_x;
                    gCurFile->cursor.y = range.end_y;
                }
                gCurFile->sx = editorRowCxToRx(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
                if (c == ARROW_UP || c == ARROW_DOWN) {
                    editorMoveCursor(c);
                }
                gCurFile->cursor.is_selected = false;
            } else {
                if (gCurFile->bracket_autocomplete) {
                    if (ARROW_RIGHT)
                        gCurFile->bracket_autocomplete--;
                    else
                        gCurFile->bracket_autocomplete = 0;
                }
                editorMoveCursor(c);
            }
            break;

        case SHIFT_UP:
        case SHIFT_DOWN:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            gCurFile->cursor.is_selected = true;
            gCurFile->bracket_autocomplete = 0;
            editorMoveCursor(c - 9);
            break;

        case CTRL_HOME:
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            gCurFile->cursor.y = 0;
            gCurFile->cursor.x = 0;
            gCurFile->sx = 0;
            break;

        case CTRL_END:
            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;
            gCurFile->cursor.y = gCurFile->num_rows - 1;
            gCurFile->cursor.x = gCurFile->row[gCurFile->num_rows - 1].size;
            gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                           gCurFile->cursor.x);
            break;

        // Action: Copy Line Up
        // Action: Copy Line Down
        case SHIFT_ALT_UP:
        case SHIFT_ALT_DOWN:
            should_record_action = true;
            gCurFile->cursor.is_selected = false;
            action->old_cursor.is_selected = 0;
            editorInsertRow(gCurFile->cursor.y,
                            gCurFile->row[gCurFile->cursor.y].data,
                            gCurFile->row[gCurFile->cursor.y].size);

            action->added_range.start_x =
                gCurFile->row[gCurFile->cursor.y].size;
            action->added_range.start_y = gCurFile->cursor.y;
            action->added_range.end_x =
                gCurFile->row[gCurFile->cursor.y + 1].size;
            action->added_range.end_y = gCurFile->cursor.y + 1;
            editorCopyText(&action->added_text, action->added_range);

            if (c == SHIFT_ALT_DOWN)
                gCurFile->cursor.y++;
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
                if (range.end_y == gCurFile->num_rows - 1)
                    break;
            }

            should_record_action = true;

            int old_cx = gCurFile->cursor.x;
            int old_cy = gCurFile->cursor.y;
            int old_select_x = gCurFile->cursor.select_x;
            int old_select_y = gCurFile->cursor.select_y;

            range.start_x = 0;
            int paste_x = 0;
            if (c == ALT_UP) {
                range.start_y--;
                range.end_x = gCurFile->row[range.end_y].size;
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
                paste_x = gCurFile->row[gCurFile->cursor.y].size;
                old_cy++;
                old_select_y++;
            }

            range.start_x = paste_x;
            range.start_y = gCurFile->cursor.y;
            editorPasteText(&action->added_text, paste_x, gCurFile->cursor.y);
            range.end_x = gCurFile->cursor.x;
            range.end_y = gCurFile->cursor.y;
            action->added_range = range;

            gCurFile->cursor.x = old_cx;
            gCurFile->cursor.y = old_cy;
            gCurFile->cursor.select_x = old_select_x;
            gCurFile->cursor.select_y = old_select_y;
        } break;

        // Mouse input
        case MOUSE_PRESSED: {
            int field = getMousePosField(x, y);
            if (field != FIELD_TEXT) {
                should_scroll = false;
                if (field == FIELD_TOP_STATUS) {
                    editorChangeToFile(getClickedFile(x));
                }
                break;
            }

            pressed = true;
            curr_x = x;
            curr_y = y;

            gCurFile->cursor.is_selected = false;
            gCurFile->bracket_autocomplete = 0;

            if (!mousePosToEditorPos(&x, &y))
                break;

            gCurFile->cursor.y = y;
            gCurFile->cursor.x = editorRowRxToCx(&gCurFile->row[y], x);
            gCurFile->sx = x;
            break;
        }

        case MOUSE_RELEASED:
        case MOUSE_MOVE:
            if (!pressed) {
                should_scroll = false;
                break;
            }

            if (moveMouse(x, y)) {
                curr_x = x;
                curr_y = y;
            }

            if (c == MOUSE_RELEASED) {
                pressed = false;
                if (gCurFile->cursor.x == gCurFile->cursor.select_x &&
                    gCurFile->cursor.y == gCurFile->cursor.select_y) {
                    gCurFile->cursor.is_selected = false;
                }
            }

            break;

        // Scroll up
        case WHEEL_UP:
        case CTRL_UP:
            should_scroll = false;
            scrollUp(c == WHEEL_UP ? 3 : 1);
            if (pressed)
                moveMouse(curr_x, curr_y);
            break;

        // Scroll down
        case WHEEL_DOWN:
        case CTRL_DOWN:
            should_scroll = false;
            scrollDown(c == WHEEL_DOWN ? 3 : 1);
            if (pressed)
                moveMouse(curr_x, curr_y);
            break;

        // Action: Input
        default: {
            if (c == UNKNOWN ||
                ((c & 0x80) == 0 && (!isprint(c) && c != '\t'))) {
                should_scroll = false;
                break;
            }

            should_record_action = true;

            getSelectStartEnd(&action->deleted_range);

            if (gCurFile->cursor.is_selected) {
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                gCurFile->cursor.is_selected = false;
            }

            int x_offset = 0;
            action->added_range.start_x = gCurFile->cursor.x;
            action->added_range.start_y = gCurFile->cursor.y;

            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            if (!CONVAR_GETINT(bracket)) {
                editorInsertChar(c);
            } else if (close_bracket) {
                editorInsertChar(c);
                editorInsertChar(close_bracket);
                x_offset = 1;
                gCurFile->cursor.x--;
                gCurFile->bracket_autocomplete++;
            } else if (open_bracket) {
                if (gCurFile->bracket_autocomplete &&
                    gCurFile->row[gCurFile->cursor.y]
                            .data[gCurFile->cursor.x] == c) {
                    gCurFile->bracket_autocomplete--;
                    x_offset = -1;
                    gCurFile->cursor.x++;
                } else {
                    editorInsertChar(c);
                }
            } else if (c == '\'' || c == '"') {
                if (gCurFile->row[gCurFile->cursor.y]
                        .data[gCurFile->cursor.x] != c) {
                    editorInsertChar(c);
                    editorInsertChar(c);
                    x_offset = 1;
                    gCurFile->cursor.x--;
                    gCurFile->bracket_autocomplete++;
                } else if (gCurFile->bracket_autocomplete &&
                           gCurFile->row[gCurFile->cursor.y]
                                   .data[gCurFile->cursor.x] == c) {
                    gCurFile->bracket_autocomplete--;
                    x_offset = -1;
                    gCurFile->cursor.x++;
                } else {
                    editorInsertChar(c);
                }
            } else {
                editorInsertChar(c);
            }

            action->added_range.end_x = gCurFile->cursor.x + x_offset;
            action->added_range.end_y = gCurFile->cursor.y;
            editorCopyText(&action->added_text, action->added_range);

            gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                           gCurFile->cursor.x);
            gCurFile->cursor.is_selected = false;

            if (x_offset == -1) {
                should_record_action = false;
            }
        } break;
    }

    if (!gCurFile->cursor.is_selected) {
        gCurFile->cursor.select_x = gCurFile->cursor.x;
        gCurFile->cursor.select_y = gCurFile->cursor.y;
    }

    if (should_record_action) {
        action->new_cursor = gCurFile->cursor;
        editorAppendAction(action);
    } else {
        editorFreeAction(action);
    }

    if (should_scroll)
        editorScroll();
    close_protect = true;
    quit_protect = true;
}
