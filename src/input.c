#include "input.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "config.h"
#include "defines.h"
#include "editor.h"
#include "file_io.h"
#include "find.h"
#include "output.h"
#include "prompt.h"
#include "select.h"
#include "status.h"
#include "terminal.h"
#include "utils.h"

void editorScrollToCursor() {
    int cols = gEditor.screen_cols - gEditor.explorer_width -
               (gCurFile->num_rows_digits + 1);
    int rx = 0;
    if (gCurFile->cursor.y < gCurFile->num_rows) {
        rx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                             gCurFile->cursor.x);
    }

    if (gCurFile->cursor.y < gCurFile->row_offset) {
        gCurFile->row_offset = gCurFile->cursor.y;
    }
    if (gCurFile->cursor.y >= gCurFile->row_offset + gEditor.display_rows) {
        gCurFile->row_offset = gCurFile->cursor.y - gEditor.display_rows + 1;
    }
    if (rx < gCurFile->col_offset) {
        gCurFile->col_offset = rx;
    }
    if (rx >= gCurFile->col_offset + cols) {
        gCurFile->col_offset = rx - cols + 1;
    }
}

void editorScrollToCursorCenter() {
    gCurFile->row_offset = gCurFile->cursor.y - gEditor.display_rows / 2;
    if (gCurFile->row_offset < 0) {
        gCurFile->row_offset = 0;
    }
}

int getMousePosField(int x, int y) {
    if (y < 0 || y >= gEditor.screen_rows)
        return FIELD_ERROR;
    if (y == 0)
        return FIELD_TOP_STATUS;
    if (y == gEditor.screen_rows - 2 && gEditor.state != EDIT_MODE)
        return FIELD_PROMPT;
    if (y == gEditor.screen_rows - 1)
        return FIELD_STATUS;
    if (x < gEditor.explorer_width)
        return FIELD_EXPLORER;
    if (x < gEditor.explorer_width + gCurFile->num_rows_digits + 1)
        return FIELD_LINENO;
    return FIELD_TEXT;
}

void mousePosToEditorPos(int* x, int* y) {
    int row = gCurFile->row_offset + *y - 1;
    if (row < 0) {
        *x = 0;
        *y = 0;
        return;
    }
    if (row >= gCurFile->num_rows) {
        *y = gCurFile->num_rows - 1;
        *x = gCurFile->row[*y].rsize;
        return;
    }

    int col = *x - gEditor.explorer_width - gCurFile->num_rows_digits - 1 +
              gCurFile->col_offset;
    if (col < 0) {
        col = 0;
    } else if (col > gCurFile->row[row].rsize) {
        col = gCurFile->row[row].rsize;
    }

    *x = col;
    *y = row;
}

void editorScroll(int dist) {
    int line = gCurFile->row_offset + dist;
    if (line < 0) {
        line = 0;
    } else if (line >= gCurFile->num_rows) {
        line = gCurFile->num_rows - 1;
    }
    gCurFile->row_offset = line;
}

void editorMoveCursor(int key) {
    const EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
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
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->sx);
            }
            break;

        case ARROW_DOWN:
            if (gCurFile->cursor.y + 1 < gCurFile->num_rows) {
                gCurFile->cursor.y++;
                gCurFile->cursor.x = editorRowRxToCx(
                    &gCurFile->row[gCurFile->cursor.y], gCurFile->sx);
            }
            break;
    }
    row = (gCurFile->cursor.y >= gCurFile->num_rows)
              ? NULL
              : &gCurFile->row[gCurFile->cursor.y];
    int row_len = row ? row->size : 0;
    if (gCurFile->cursor.x > row_len) {
        gCurFile->cursor.x = row_len;
    }
}

static int findNextCharIndex(const EditorRow* row, int index,
                             IsCharFunc is_char) {
    while (index < row->size && !is_char(row->data[index])) {
        index++;
    }
    return index;
}

static int findPrevCharIndex(const EditorRow* row, int index,
                             IsCharFunc is_char) {
    while (index > 0 && !is_char(row->data[index - 1])) {
        index--;
    }
    return index;
}

static void editorMoveCursorWordLeft() {
    if (gCurFile->cursor.x == 0) {
        if (gCurFile->cursor.y == 0)
            return;
        editorMoveCursor(ARROW_LEFT);
    }

    const EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    gCurFile->cursor.x =
        findPrevCharIndex(row, gCurFile->cursor.x, isIdentifierChar);
    gCurFile->cursor.x =
        findPrevCharIndex(row, gCurFile->cursor.x, isNonIdentifierChar);
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

    const EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    gCurFile->cursor.x =
        findNextCharIndex(row, gCurFile->cursor.x, isIdentifierChar);
    gCurFile->cursor.x =
        findNextCharIndex(row, gCurFile->cursor.x, isNonIdentifierChar);
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}

static void editorSelectWord(const EditorRow* row, int cx, IsCharFunc is_char) {
    gCurFile->cursor.select_x = findPrevCharIndex(row, cx, is_char);
    gCurFile->cursor.x = findNextCharIndex(row, cx, is_char);
    gCurFile->sx = editorRowCxToRx(row, gCurFile->cursor.x);
    gCurFile->cursor.is_selected = true;
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

static int handleTabClick(int x) {
    if (gEditor.loading)
        return -1;

    if (x < gEditor.explorer_width)
        return -1;

    bool has_more_files = false;
    int tab_displayed = 0;
    int len = gEditor.explorer_width;
    if (gEditor.tab_offset != 0) {
        if (x == 0) {
            gEditor.tab_offset--;
            return -1;
        }
        len++;
    }

    for (int i = 0; i < gEditor.file_count; i++) {
        if (i < gEditor.tab_offset)
            continue;

        const EditorFile* file = &gEditor.files[i];
        int buf_len = strlen(file->filename ? file->filename : "Untitled") + 2;

        if (file->dirty)
            buf_len++;

        if (gEditor.screen_cols - len < buf_len ||
            (i != gEditor.file_count - 1 &&
             gEditor.screen_cols - len == buf_len)) {
            has_more_files = true;
            if (tab_displayed == 0) {
                // Display at least one tab
                buf_len = gEditor.screen_cols - len - 1;
            } else {
                break;
            }
        }

        len += buf_len;
        if (len > x)
            return i;

        tab_displayed++;
    }
    if (has_more_files)
        gEditor.tab_offset++;
    return -1;
}

static bool moveMouse(int x, int y) {
    if (getMousePosField(x, y) != FIELD_TEXT)
        return false;
    mousePosToEditorPos(&x, &y);
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
    static struct timeval prev_click_time = {0};
    static int mouse_click = 0;
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
#ifdef _DEBUG
            editorFree();
#endif
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
        case CTRL_KEY('n'):
            should_scroll = false;
            editorSave(gCurFile, 1);
            break;

        // Open file
        case CTRL_KEY('o'):
            should_scroll = false;
            editorOpenFilePrompt();
            break;

        case HOME_KEY:
        case SHIFT_HOME: {
            int start_x = findNextCharIndex(&gCurFile->row[gCurFile->cursor.y],
                                            0, isNonSpace);
            if (start_x == gCurFile->cursor.x)
                start_x = 0;
            gCurFile->cursor.x = start_x;
            gCurFile->sx =
                editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], start_x);
            gCurFile->cursor.is_selected = c == (SHIFT_HOME);
            gCurFile->bracket_autocomplete = 0;
        } break;

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
        SELECT_ALL:
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

            should_scroll = false;
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

            char deleted_char = '\0';
            if (gCurFile->cursor.x != 0)
                deleted_char = gCurFile->row[gCurFile->cursor.y]
                                   .data[gCurFile->cursor.x - 1];
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
            if (gCurFile->num_rows == 1 && gCurFile->row[0].size == 0)
                break;

            should_record_action = true;
            editorFreeClipboardContent(&gEditor.clipboard);

            if (!gCurFile->cursor.is_selected) {
                // Copy line
                EditorSelectRange range = {
                    findNextCharIndex(&gCurFile->row[gCurFile->cursor.y], 0,
                                      isNonSpace),
                    gCurFile->cursor.y, gCurFile->row[gCurFile->cursor.y].size,
                    gCurFile->cursor.y};
                editorCopyText(&gEditor.clipboard, range);

                // Delete line
                range.start_x = 0;
                if (gCurFile->num_rows != 1) {
                    if (gCurFile->cursor.y == gCurFile->num_rows - 1) {
                        range.start_y--;
                        range.start_x = gCurFile->row[range.start_y].size;
                    } else {
                        range.end_y++;
                        range.end_x = 0;
                    }
                }

                action->deleted_range = range;
                editorCopyText(&action->deleted_text, action->deleted_range);
                editorDeleteText(action->deleted_range);
                break;
            }
            getSelectStartEnd(&action->deleted_range);
            editorCopyText(&action->deleted_text, action->deleted_range);
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
                    findNextCharIndex(&gCurFile->row[gCurFile->cursor.y], 0,
                                      isNonSpace),
                    gCurFile->cursor.y, gCurFile->row[gCurFile->cursor.y].size,
                    gCurFile->cursor.y};
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

        // Select word
        case CTRL_KEY('d'): {
            const EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
            if (!isIdentifierChar(row->data[gCurFile->cursor.x])) {
                should_scroll = false;
                break;
            }
            editorSelectWord(row, gCurFile->cursor.x, isNonIdentifierChar);
        } break;

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
                if (gCurFile->row[gCurFile->cursor.y].size == 0) {
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
                if (gCurFile->row[gCurFile->cursor.y].size == 0) {
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
            editorInsertRow(gCurFile, gCurFile->cursor.y,
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
                mouse_click = 0;
                if (field == FIELD_TOP_STATUS) {
                    editorChangeToFile(handleTabClick(x));
                } else if (field == FIELD_EXPLORER) {
                    if (y > gEditor.explorer_last_line - gEditor.explorer_offset)
                        break;
                    EditorExplorerNode* node = editorExplorerSearch(y - 1 + gEditor.explorer_offset);
                    EditorFile file = {0};
                    if (!node)
                        break;
                    if (node->is_directory) {
                        node->is_open ^= 1;
                    } else if (editorOpen(&file, node->filename)) {
                        int index = editorAddFile(&file);
                        // hack: refresh screen to update gEditor.tab_displayed
                        editorRefreshScreen();
                        editorChangeToFile(index);
                    }
                }
                break;
            }

            struct timeval click_time;
            gettimeofday(&click_time, NULL);
            int64_t time_diff =
                (click_time.tv_sec - prev_click_time.tv_sec) * 1000000 +
                (click_time.tv_usec - prev_click_time.tv_usec);
            if (x == curr_x && y == curr_y && time_diff / 1000 < 500) {
                mouse_click++;
            } else {
                mouse_click = 1;
            }
            prev_click_time = click_time;

            pressed = true;
            curr_x = x;
            curr_y = y;

            gCurFile->bracket_autocomplete = 0;

            mousePosToEditorPos(&x, &y);
            int cx = editorRowRxToCx(&gCurFile->row[y], x);

            switch (mouse_click % 4) {
                case 1:
                    // Mouse to pos
                    gCurFile->cursor.is_selected = false;
                    gCurFile->cursor.y = y;
                    gCurFile->cursor.x = cx;
                    gCurFile->sx = x;
                    break;
                case 2: {
                    // Select word
                    const EditorRow* row = &gCurFile->row[y];
                    if (row->size == 0)
                        break;
                    if (cx == row->size)
                        cx--;

                    IsCharFunc is_char;
                    if (isspace(row->data[cx])) {
                        is_char = isNonSpace;
                    } else if (isIdentifierChar(row->data[cx])) {
                        is_char = isNonIdentifierChar;
                    } else {
                        is_char = isNonSeparator;
                    }
                    editorSelectWord(row, cx, is_char);
                } break;
                case 3:
                    // Select line
                    if (gCurFile->cursor.y == gCurFile->num_rows - 1) {
                        gCurFile->cursor.x =
                            gCurFile->row[gCurFile->cursor.y].size;
                        gCurFile->cursor.select_x = 0;
                        gCurFile->sx = editorRowCxToRx(&gCurFile->row[y],
                                                       gCurFile->cursor.x);
                    } else {
                        gCurFile->cursor.x = 0;
                        gCurFile->cursor.y++;
                        gCurFile->cursor.select_x = 0;
                        gCurFile->sx = 0;
                    }
                    gCurFile->cursor.is_selected = true;
                    break;
                case 0:
                    goto SELECT_ALL;
            }
        } break;

        case MOUSE_RELEASED:
            should_scroll = false;
            pressed = false;
            break;

        case MOUSE_MOVE:
            should_scroll = false;
            if (moveMouse(x, y)) {
                curr_x = x;
                curr_y = y;
            }
            break;

        // Scroll up
        case WHEEL_UP: {
            int field = getMousePosField(x, y);
            should_scroll = false;
            if (field != FIELD_TEXT && field != FIELD_LINENO) {
                if (field == FIELD_TOP_STATUS) {
                    if (gEditor.tab_offset > 0)
                        gEditor.tab_offset--;
                } else if (field == FIELD_EXPLORER) {
                    if (gEditor.explorer_offset > 0) {
                        gEditor.explorer_offset =
                            (gEditor.explorer_offset - 3)
                                ? (gEditor.explorer_offset - 3)
                                : 0;
                    }
                }
                break;
            }
        }
        // fall through
        case CTRL_UP:
            should_scroll = false;
            editorScroll(-(c == WHEEL_UP ? 3 : 1));
            if (pressed)
                moveMouse(curr_x, curr_y);
            break;

        // Scroll down
        case WHEEL_DOWN: {
            int field = getMousePosField(x, y);
            should_scroll = false;
            if (field != FIELD_TEXT && field != FIELD_LINENO) {
                if (field == FIELD_TOP_STATUS) {
                    handleTabClick(gEditor.screen_cols);
                } else if (field == FIELD_EXPLORER && gEditor.explorer_last_line - gEditor.explorer_offset > gEditor.display_rows) {
                    gEditor.explorer_offset += 3;
                }
                break;
            }
        }
        // fall through
        case CTRL_DOWN:
            should_scroll = false;
            editorScroll(c == WHEEL_DOWN ? 3 : 1);
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

    if (gCurFile->cursor.x == gCurFile->cursor.select_x &&
        gCurFile->cursor.y == gCurFile->cursor.select_y) {
        gCurFile->cursor.is_selected = false;
    }

    if (!gCurFile->cursor.is_selected) {
        gCurFile->cursor.select_x = gCurFile->cursor.x;
        gCurFile->cursor.select_y = gCurFile->cursor.y;
    }

    if (c != MOUSE_PRESSED && c != MOUSE_RELEASED)
        mouse_click = 0;

    if (should_record_action) {
        action->new_cursor = gCurFile->cursor;
        editorAppendAction(action);
    } else {
        editorFreeAction(action);
    }

    if (should_scroll)
        editorScrollToCursor();
    close_protect = true;
    quit_protect = true;
}
