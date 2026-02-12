#include "input.h"

#include <ctype.h>

#include "config.h"
#include "editor.h"
#include "file_io.h"
#include "output.h"
#include "prompt.h"
#include "select.h"
#include "terminal.h"
#include "unicode.h"
#include "utils.h"

static void editorExplorerNodeClicked(void) {
    EditorExplorerNode* node = NULL;
    EditorFile file = {0};

    if (gEditor.explorer.selected_index < (int)gEditor.explorer.flatten.size)
        node = gEditor.explorer.flatten.data[gEditor.explorer.selected_index];

    if (!node)
        return;
    if (node->is_directory) {
        node->is_open ^= 1;
        editorExplorerRefresh();
    } else if (editorOpen(&file, node->filename) == OPEN_FILE) {
        editorAddFile(&file);
    }
}

static void editorExplorerScrollToSelect(void) {
    if (gEditor.explorer.offset > gEditor.explorer.selected_index) {
        gEditor.explorer.offset = gEditor.explorer.selected_index;
    } else if ((int)gEditor.explorer.selected_index >=
               gEditor.explorer.offset + gEditor.display_rows) {
        gEditor.explorer.offset =
            gEditor.explorer.selected_index - gEditor.display_rows + 1;
    }

    if (gEditor.explorer.offset < 0) {
        gEditor.explorer.offset = 0;
    }
}

void editorExplorerShow(void) {
    if (gEditor.explorer.width == 0) {
        gEditor.explorer.width = gEditor.explorer.prefered_width
                                     ? gEditor.explorer.prefered_width
                                     : CONVAR_GETINT(ex_default_width);
    }
}

static bool editorExplorerProcessKeypress(EditorInput input) {
    if (input.type == CHAR_INPUT) {
        if (!gEditor.explorer.node)
            return true;

        uint32_t unicode = input.data.unicode;
        if (unicode > 255)
            return true;

        char c = tolower(unicode);
        size_t index = gEditor.explorer.selected_index + 1;
        for (size_t i = 0; i < gEditor.explorer.flatten.size; i++) {
            index = index % gEditor.explorer.flatten.size;
            if (tolower(getBaseName(
                    gEditor.explorer.flatten.data[index]->filename)[0]) == c) {
                gEditor.explorer.selected_index = index;
                editorExplorerScrollToSelect();
                break;
            }
            index++;
        }
        return true;
    }

    switch (input.type) {
        case WHEEL_UP:
            if (getMousePosField(input.data.cursor.x, input.data.cursor.y) !=
                FIELD_EXPLORER)
                return false;
            if (gEditor.explorer.offset > 0) {
                gEditor.explorer.offset = (gEditor.explorer.offset - 3) > 0
                                              ? (gEditor.explorer.offset - 3)
                                              : 0;
            }
            break;

        case WHEEL_DOWN:
            if (getMousePosField(input.data.cursor.x, input.data.cursor.y) !=
                FIELD_EXPLORER)
                return false;
            if ((int)gEditor.explorer.flatten.size - gEditor.explorer.offset >
                gEditor.display_rows) {
                gEditor.explorer.offset += 3;
            }
            break;

        case ARROW_UP:
            if (gEditor.explorer.selected_index <= 0)
                break;
            gEditor.explorer.selected_index--;
            editorExplorerScrollToSelect();
            break;

        case ARROW_DOWN:
            if (gEditor.explorer.selected_index + 1 >=
                (int)gEditor.explorer.flatten.size)
                break;
            gEditor.explorer.selected_index++;
            editorExplorerScrollToSelect();
            break;

        case HOME_KEY:
            gEditor.explorer.selected_index = 0;
            editorExplorerScrollToSelect();
            break;

        case END_KEY:
            gEditor.explorer.selected_index = gEditor.explorer.flatten.size - 1;
            editorExplorerScrollToSelect();
            break;

        case PAGE_UP:
            if (gEditor.explorer.selected_index != gEditor.explorer.offset) {
                gEditor.explorer.selected_index = gEditor.explorer.offset;
            } else {
                gEditor.explorer.selected_index -= gEditor.display_rows;
                if (gEditor.explorer.selected_index < 0) {
                    gEditor.explorer.selected_index = 0;
                }
            }
            editorExplorerScrollToSelect();
            break;

        case PAGE_DOWN:
            if (gEditor.explorer.selected_index !=
                gEditor.explorer.offset + gEditor.display_rows - 1) {
                gEditor.explorer.selected_index =
                    gEditor.explorer.offset + gEditor.display_rows - 1;
            } else {
                gEditor.explorer.selected_index += gEditor.display_rows;
            }

            if (gEditor.explorer.selected_index >=
                (int)gEditor.explorer.flatten.size) {
                gEditor.explorer.selected_index =
                    gEditor.explorer.flatten.size - 1;
            }
            editorExplorerScrollToSelect();
            break;

        case '\r':
            editorExplorerNodeClicked();
            break;

        case MOUSE_PRESSED:
            if (getMousePosField(input.data.cursor.x, input.data.cursor.y) !=
                FIELD_EXPLORER) {
                gEditor.state = EDIT_MODE;
                return false;
            }

            if (input.data.cursor.x == gEditor.explorer.width - 1) {
                return false;
            }

            if (input.data.cursor.y == 0) {
                gEditor.state = EXPLORER_MODE;
                break;
            }

            if (input.data.cursor.y >
                (int)gEditor.explorer.flatten.size - gEditor.explorer.offset)
                break;
            gEditor.explorer.selected_index =
                input.data.cursor.y - 1 + gEditor.explorer.offset;
            editorExplorerNodeClicked();
            break;

        case CTRL_KEY('q'):
            if (gEditor.file_count == 0) {
#ifdef _DEBUG
                editorFree();
#endif
                exit(EXIT_SUCCESS);
            }
            return false;

        case MOUSE_MOVE:
        case MOUSE_RELEASED:
        case SCROLL_PRESSED:
        case SCROLL_RELEASED:
        case CTRL_KEY('w'):
        case CTRL_KEY('b'):
        case CTRL_KEY('['):
        case CTRL_KEY(']'):
            return false;

        case CTRL_KEY('e'):
            if (gEditor.file_count != 0) {
                gEditor.state = EDIT_MODE;
            }
            break;
    }
    return true;
}

void editorScrollToCursor(void) {
    int cols = gEditor.screen_cols - gEditor.explorer.width - LINENO_WIDTH();
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

void editorScrollToCursorCenter(void) {
    gCurFile->row_offset = gCurFile->cursor.y - gEditor.display_rows / 2;
    if (gCurFile->row_offset < 0) {
        gCurFile->row_offset = 0;
    }
}

int getMousePosField(int x, int y) {
    if (y < 0 || y >= gEditor.screen_rows)
        return FIELD_ERROR;
    if (y == 0)
        return x < gEditor.explorer.width ? FIELD_EXPLORER : FIELD_TOP_STATUS;
    if (y == gEditor.screen_rows - 2 && gEditor.state != EDIT_MODE &&
        gEditor.state != EXPLORER_MODE)
        return FIELD_PROMPT;
    if (y == gEditor.screen_rows - 1)
        return FIELD_STATUS;
    if (x < gEditor.explorer.width)
        return FIELD_EXPLORER;
    if (gEditor.file_count == 0)
        return FIELD_EMPTY;
    if (x < gEditor.explorer.width + LINENO_WIDTH())
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

    int col =
        *x - gEditor.explorer.width - LINENO_WIDTH() + gCurFile->col_offset;
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

static int findNextCharIndex(const EditorRow* row,
                             int index,
                             IsCharFunc is_char) {
    while (index < row->size && !is_char(row->data[index])) {
        index++;
    }
    return index;
}

static int findPrevCharIndex(const EditorRow* row,
                             int index,
                             IsCharFunc is_char) {
    while (index > 0 && !is_char(row->data[index - 1])) {
        index--;
    }
    return index;
}

static void editorMoveCursorWordLeft(void) {
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

static void editorMoveCursorWordRight(void) {
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

static void editorSelectLine(int row) {
    if (row < 0 || row >= gCurFile->num_rows)
        return;

    gCurFile->cursor.is_selected = true;
    gCurFile->cursor.select_x = 0;
    gCurFile->cursor.select_y = row;
    gCurFile->bracket_autocomplete = 0;

    if (row < gCurFile->num_rows - 1) {
        gCurFile->cursor.y = row + 1;
        gCurFile->cursor.x = 0;
    } else {
        gCurFile->cursor.y = row;
        gCurFile->cursor.x = gCurFile->row[row].size;
        if (gCurFile->cursor.x == 0) {
            gCurFile->cursor.is_selected = false;
        }
    }

    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}

static void editorSelectAll(void) {
    if (gCurFile->num_rows == 1 && gCurFile->row[0].size == 0)
        return;
    gCurFile->cursor.is_selected = true;
    gCurFile->bracket_autocomplete = 0;
    gCurFile->cursor.y = gCurFile->num_rows - 1;
    gCurFile->cursor.x = gCurFile->row[gCurFile->num_rows - 1].size;
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
    gCurFile->cursor.select_y = 0;
    gCurFile->cursor.select_x = 0;
}

static int handleTabClick(int x) {
    if (gEditor.state == LOADING_MODE)
        return -1;

    if (x < gEditor.explorer.width)
        return -1;

    bool has_more_files = false;
    int tab_displayed = 0;
    int len = gEditor.explorer.width;
    if (gEditor.tab_offset != 0) {
        if (x == gEditor.explorer.width) {
            gEditor.tab_offset--;
            return -1;
        }
        len++;
    }

    for (int i = 0; i < gEditor.file_count; i++) {
        if (i < gEditor.tab_offset)
            continue;

        const EditorFile* file = &gEditor.files[i];
        int tab_width;
        if (file->filename) {
            tab_width = strUTF8Width(getBaseName(file->filename));
        } else {
            // Untitled-%d
            tab_width = strlen("Untitled-") + getDigit(i + 1);
        }

        // Add * if file is dirty
        if (file->dirty) {
            tab_width++;
        }

        // Add padding
        tab_width += 2;

        if (gEditor.screen_cols - len < tab_width ||
            (i != gEditor.file_count - 1 &&
             gEditor.screen_cols - len == tab_width)) {
            has_more_files = true;
            if (tab_displayed == 0) {
                // Display at least one tab
                tab_width = gEditor.screen_cols - len - 1;
            } else {
                break;
            }
        }

        len += tab_width;
        if (len > x)
            return i;

        tab_displayed++;
    }
    if (has_more_files)
        gEditor.tab_offset++;
    return -1;
}

static void editorMoveMouse(int x, int y) {
    mousePosToEditorPos(&x, &y);
    gCurFile->cursor.is_selected = true;
    gCurFile->cursor.x = editorRowRxToCx(&gCurFile->row[y], x);
    gCurFile->cursor.y = y;
    gCurFile->sx = x;
}

static void editorCloseFile(int index) {
    if (index < 0 || index > gEditor.file_count) {
        return;
    }

    editorRemoveFile(index);
    if (gEditor.file_count == 0) {
        gEditor.state = EXPLORER_MODE;
        editorExplorerShow();
    }

    if (index < gEditor.file_index ||
        (gEditor.file_index == index && index == gEditor.file_count)) {
        editorChangeToFile(gEditor.file_index - 1);
    }
}

void editorProcessKeypress(void) {
    static int mouse_pressed = FIELD_EMPTY;
    static int64_t prev_click_time = 0;
    static int mouse_click = 0;
    static int curr_x = 0;
    static int curr_y = 0;
    static int pressed_row = 0;  // For select line drag
    static EditorInput pending_input = {.type = UNKNOWN};

    // Check if there's a pending input from previous call
    EditorInput input;
    if (pending_input.type != UNKNOWN) {
        input = pending_input;
        pending_input.type = UNKNOWN;
    } else {
        input = editorReadKey();
    }

    // Global keybinds
    switch (input.type) {
        // Prompt
        case CTRL_KEY('p'):
            editorOpenConfigPrompt();
            return;

        // Open file
        case CTRL_KEY('o'):
            editorOpenFilePrompt();
            return;

        // New tab
        case CTRL_KEY('n'): {
            EditorFile file;
            editorNewUntitledFile(&file);
            if (editorAddFile(&file) != -1) {
                gEditor.state = EDIT_MODE;
            }
            return;
        }
    }

    // TODO: Don't clear status message on all unused inputs
    if (input.type != MOUSE_RELEASED) {
        editorMsgClear();
    }

    if (gEditor.state == EXPLORER_MODE &&
        editorExplorerProcessKeypress(input)) {
        editorFreeInput(&input);
        return;
    }

    if (gEditor.file_count == 0) {
        gEditor.state = EXPLORER_MODE;
        editorFreeInput(&input);
        return;
    }

    bool should_scroll = true;

    bool has_edit = false;
    Edit edit = {0};
    EditorCursor old_cursor = gCurFile->cursor;
    EditorCursor new_cursor = gCurFile->cursor;
    bool should_set_cursor = false;
    int next_bracket_autocomplete = gCurFile->bracket_autocomplete;

    int c = input.type;
    switch (c) {
        // Action: Newline
        case '\r': {
            has_edit = true;

            edit.x = gCurFile->cursor.x;
            edit.y = gCurFile->cursor.y;

            EditorSelectRange delete_range = {0};
            if (gCurFile->cursor.is_selected) {
                getSelectStartEnd(&gCurFile->cursor, &delete_range);
                editorCopyText(&edit.before, delete_range);
            }

            editorFreeClipboardContent(&edit.after);
            editorClipboardAppendNewline(&edit.after);
            editorClipboardAppendNewline(&edit.after);

            if (CONVAR_GETINT(autoindent)) {
                const EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
                bool should_indent;
                if (gCurFile->cursor.x < row->size) {
                    should_indent = false;
                    for (int i = gCurFile->cursor.x; i < row->size; i++) {
                        if (row->data[i] != ' ' && row->data[i] != '\t') {
                            should_indent = true;
                            break;
                        }
                    }
                } else {
                    should_indent = true;
                }

                if (should_indent) {
                    int i = 0;
                    while (i < row->size &&
                           (row->data[i] == ' ' || row->data[i] == '\t')) {
                        i++;
                    }
                    if (i > 0) {
                        editorClipboardAppendAt(&edit.after, 1, row->data,
                                                (size_t)i);
                    }

                    // TODO: language specific auto indent
                    bool should_inc = false;
                    if (row->size > 0) {
                        char prev = row->data[row->size - 1];
                        if (prev == ':') {
                            // Python
                            should_inc = true;
                        } else if (prev == '{') {
                            // C
                            if (gCurFile->cursor.x < row->size) {
                                should_inc =
                                    (row->data[gCurFile->cursor.x] != '}');
                            } else {
                                should_inc = true;
                            }
                        }
                    }

                    if (should_inc) {
                        if (CONVAR_GETINT(whitespace)) {
                            editorClipboardAppendAtRepeat(
                                &edit.after, edit.after.size - 1, ' ',
                                CONVAR_GETINT(tabsize));
                        } else {
                            editorClipboardAppendChar(&edit.after, '\t');
                        }
                    }
                }
            }

            gCurFile->cursor.is_selected = false;
            next_bracket_autocomplete = 0;
            should_set_cursor = true;
            new_cursor = gCurFile->cursor;
            new_cursor.x = edit.after.lines[1].size;
            new_cursor.y = gCurFile->cursor.y + 1;
        } break;

        // Quit editor
        case CTRL_KEY('q'): {
            should_scroll = false;

            int dirty = 0;
            for (int i = 0; i < gEditor.file_count; i++) {
                if (gEditor.files[i].dirty) {
                    dirty++;
                }
            }

            if (dirty > 0) {
                if (dirty == 1) {
                    editorMsg("File has unsaved changes.");
                } else {
                    editorMsg("Files have unsaved changes.");
                }
                editorMsg("Press quit again to quit anyway.");
                editorRefreshScreen();

                // Read next key to check if it's a repeat
                EditorInput next_input = editorReadKey();
                if (next_input.type != c) {
                    pending_input = next_input;
                    break;
                }
            }
#ifdef _DEBUG
            editorFree();
#endif
            exit(EXIT_SUCCESS);
        }

        // Close current file
        case CTRL_KEY('w'): {
            should_scroll = false;

            if (gEditor.files[gEditor.file_index].dirty) {
                editorMsg("File has unsaved changes.");
                editorMsg("Press close again to close file anyway.");
                editorRefreshScreen();

                // Read next key to check if it's a repeat
                EditorInput next_input = editorReadKey();
                if (next_input.type != c) {
                    pending_input = next_input;
                    break;
                }
            }
            editorCloseFile(gEditor.file_index);
            break;
        }

        // Save
        case CTRL_KEY('s'):
            should_scroll = false;
            if (gCurFile->dirty || !gCurFile->filename)
                editorSave(gCurFile, 0);
            break;

        // Save all
        case ALT_KEY('s'):
            should_scroll = false;
            for (int i = 0; i < gEditor.file_count; i++) {
                if (gEditor.files[i].dirty || !gEditor.files[i].filename) {
                    editorSave(&gEditor.files[i], 0);
                }
            }
            break;

        // Save as
        case ALT_KEY('a'):
            // Alt+A
            should_scroll = false;
            editorSave(gCurFile, 1);
            break;

        // Toggle explorer
        case CTRL_KEY('b'):
            should_scroll = false;
            if (gEditor.explorer.width == 0) {
                editorExplorerShow();
            } else {
                gEditor.explorer.width = 0;
                gEditor.state = EDIT_MODE;
            }
            break;

        // Focus explorer
        case CTRL_KEY('e'):
            should_scroll = false;
            gEditor.state = EXPLORER_MODE;
            editorExplorerShow();
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
            gCurFile->cursor.is_selected = (c == (SHIFT_HOME));
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

        // Select line
        case CTRL_KEY('l'):
            editorSelectLine(gCurFile->cursor.y);
            break;

        // Select all
        case CTRL_KEY('a'):
            should_scroll = false;
            editorSelectAll();
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

            has_edit = true;

            if (gCurFile->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&gCurFile->cursor, &range);
                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(&edit.before, range);
                editorFreeClipboardContent(&edit.after);
                should_set_cursor = true;
                new_cursor = gCurFile->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
                gCurFile->cursor.is_selected = false;
                break;
            }

            int cx = gCurFile->cursor.x;
            int cy = gCurFile->cursor.y;
            int start_x = cx;
            int start_y = cy;
            int end_x = cx;
            int end_y = cy;
            int new_x = cx;
            int new_y = cy;
            int bracket_delta = 0;

            EditorRow* row = &gCurFile->row[cy];

            if (c == DEL_KEY) {
                if (cx < gCurFile->row[cy].size) {
                    end_x = editorRowNextUTF8(row, cx);
                } else if (cy + 1 < gCurFile->num_rows) {
                    end_x = 0;
                    end_y = cy + 1;
                }
            } else {
                if (cx > 0) {
                    start_x = editorRowPreviousUTF8(row, cx);
                    char deleted_char = row->data[start_x];
                    bool should_delete_tab =
                        CONVAR_GETINT(backspace) && deleted_char == ' ';
                    if (should_delete_tab) {
                        bool only_spaces = true;
                        for (int i = 0; i < start_x; i++) {
                            if (row->data[i] != ' ' && row->data[i] != '\t') {
                                only_spaces = false;
                                break;
                            }
                        }
                        if (only_spaces) {
                            int rx = editorRowCxToRx(row, start_x);
                            while (rx % CONVAR_GETINT(tabsize) != 0 &&
                                   start_x > 0 &&
                                   row->data[start_x - 1] == ' ') {
                                start_x--;
                                rx--;
                            }
                        }
                    }
                    new_x = start_x;
                } else if (cy > 0) {
                    start_y = cy - 1;
                    start_x = gCurFile->row[start_y].size;
                    new_y = start_y;
                    new_x = start_x;
                }
            }

            if (gCurFile->bracket_autocomplete && cx > 0 && cx < row->size) {
                char left = row->data[cx - 1];
                char right = row->data[cx];
                bool match = isCloseBracket(right) == left ||
                             (left == '\'' && right == '\'') ||
                             (left == '"' && right == '"');
                if (match && c != DEL_KEY) {
                    end_x = editorRowNextUTF8(row, cx);
                    start_x = editorRowPreviousUTF8(row, cx);
                    new_x = start_x;
                    bracket_delta = -1;
                } else if (match && c == DEL_KEY) {
                    end_x = editorRowNextUTF8(row, cx);
                    bracket_delta = -1;
                }
            }

            EditorSelectRange range = {start_x, start_y, end_x, end_y};
            edit.x = range.start_x;
            edit.y = range.start_y;
            editorCopyText(&edit.before, range);
            editorFreeClipboardContent(&edit.after);
            next_bracket_autocomplete += bracket_delta;
            if (next_bracket_autocomplete < 0)
                next_bracket_autocomplete = 0;
            should_set_cursor = true;
            new_cursor = gCurFile->cursor;
            new_cursor.is_selected = false;
            new_cursor.x = new_x;
            new_cursor.y = new_y;
        } break;

        // Action: Cut
        case CTRL_KEY('x'): {
            if (gCurFile->num_rows == 1 && gCurFile->row[0].size == 0)
                break;

            has_edit = true;
            editorFreeClipboardContent(&gEditor.clipboard);

            if (!gCurFile->cursor.is_selected) {
                // Copy line
                editorCopyLine(&gEditor.clipboard, gCurFile->cursor.y);
                gEditor.copy_line = true;

                // Delete line
                EditorSelectRange range = {
                    0, gCurFile->cursor.y,
                    gCurFile->row[gCurFile->cursor.y].size, gCurFile->cursor.y};
                if (gCurFile->num_rows != 1) {
                    if (gCurFile->cursor.y == gCurFile->num_rows - 1) {
                        range.start_y--;
                        range.start_x = gCurFile->row[range.start_y].size;
                    } else {
                        range.end_y++;
                        range.end_x = 0;
                    }
                }

                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(&edit.before, range);
                editorFreeClipboardContent(&edit.after);
                should_set_cursor = true;
                new_cursor = gCurFile->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
            } else {
                EditorSelectRange range;
                getSelectStartEnd(&gCurFile->cursor, &range);
                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(&edit.before, range);
                editorFreeClipboardContent(&edit.after);
                editorCopyText(&gEditor.clipboard, range);
                gEditor.copy_line = false;
                should_set_cursor = true;
                new_cursor = gCurFile->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
            }
            editorCopyToSysClipboard(&gEditor.clipboard, gCurFile->newline);
        } break;

        // Copy
        case CTRL_KEY('c'): {
            editorFreeClipboardContent(&gEditor.clipboard);
            should_scroll = false;

            if (gCurFile->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&gCurFile->cursor, &range);
                gEditor.copy_line = false;
                editorCopyText(&gEditor.clipboard, range);
            } else {
                editorCopyLine(&gEditor.clipboard, gCurFile->cursor.y);
                gEditor.copy_line = true;
            }
            editorCopyToSysClipboard(&gEditor.clipboard, gCurFile->newline);
        } break;

        // Action: Paste
        case PASTE_INPUT:
        case CTRL_KEY('v'): {
            EditorClipboard* clipboard =
                (c == PASTE_INPUT) ? &input.data.paste : &gEditor.clipboard;

            if (!clipboard->size)
                break;

            has_edit = true;

            bool copy_line = (c == PASTE_INPUT) ? false : gEditor.copy_line;
            EditorSelectRange delete_range = {
                gCurFile->cursor.x, gCurFile->cursor.y, gCurFile->cursor.x,
                gCurFile->cursor.y};
            if (gCurFile->cursor.is_selected) {
                getSelectStartEnd(&gCurFile->cursor, &delete_range);
                editorCopyText(&edit.before, delete_range);
            }

            edit.x = delete_range.start_x;
            edit.y = delete_range.start_y;
            editorFreeClipboardContent(&edit.after);
            if (clipboard->size > 0) {
                edit.after.size = clipboard->size;
                edit.after.lines = malloc_s(sizeof(Str) * edit.after.size);
                for (size_t i = 0; i < clipboard->size; i++) {
                    edit.after.lines[i].size = clipboard->lines[i].size;
                    if (clipboard->lines[i].size == 0) {
                        edit.after.lines[i].data = NULL;
                    } else {
                        edit.after.lines[i].data =
                            malloc_s((size_t)clipboard->lines[i].size);
                        memcpy(edit.after.lines[i].data,
                               clipboard->lines[i].data,
                               (size_t)clipboard->lines[i].size);
                    }
                }
            }

            if (!gCurFile->cursor.is_selected && copy_line) {
                edit.x = 0;
            }

            should_set_cursor = true;
            new_cursor = gCurFile->cursor;
            new_cursor.is_selected = false;
            if (edit.after.size == 0) {
                new_cursor.x = edit.x;
                new_cursor.y = edit.y;
            } else if (edit.after.size == 1) {
                new_cursor.x = edit.x + edit.after.lines[0].size;
                new_cursor.y = edit.y;
            } else {
                new_cursor.y = edit.y + (int)edit.after.size - 1;
                new_cursor.x = edit.after.lines[edit.after.size - 1].size;
            }

            gCurFile->cursor.is_selected = false;
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
            if (gCurFile->cursor.x < row->size &&
                !isIdentifierChar(row->data[gCurFile->cursor.x])) {
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
        case PAGE_DOWN: {
            gCurFile->cursor.is_selected =
                (c == SHIFT_PAGE_UP || c == SHIFT_PAGE_DOWN);
            gCurFile->bracket_autocomplete = 0;

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
        } break;

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
                getSelectStartEnd(&gCurFile->cursor, &range);

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
                    if (c == ARROW_RIGHT) {
                        gCurFile->bracket_autocomplete--;
                    } else {
                        gCurFile->bracket_autocomplete = 0;
                    }
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
            has_edit = true;
            gCurFile->cursor.is_selected = false;
            edit.x = 0;
            edit.y = gCurFile->cursor.y;
            editorCopyLine(&edit.after, gCurFile->cursor.y);
            should_set_cursor = true;
            new_cursor = gCurFile->cursor;
            new_cursor.is_selected = false;
            if (c == SHIFT_ALT_DOWN) {
                new_cursor.y++;
            }
            break;

        // Action: Move Line Up
        // Action: Move Line Down
        case ALT_UP:
        case ALT_DOWN: {
            EditorSelectRange range;
            getSelectStartEnd(&gCurFile->cursor, &range);
            if (c == ALT_UP) {
                if (range.start_y == 0)
                    break;
            } else {
                if (range.end_y == gCurFile->num_rows - 1)
                    break;
            }

            has_edit = true;

            edit.x = 0;
            edit.y = (c == ALT_UP) ? range.start_y - 1 : range.start_y;
            range.start_x = 0;
            if (c == ALT_UP) {
                range.start_y--;
                range.end_x = gCurFile->row[range.end_y].size;
            } else {
                range.end_y++;
                range.end_x = gCurFile->row[range.end_y].size;
            }
            editorCopyText(&edit.before, range);
            editorFreeClipboardContent(&edit.after);
            edit.after.size = edit.before.size;
            edit.after.lines = malloc_s(sizeof(Str) * edit.after.size);
            for (size_t i = 0; i < edit.before.size; i++) {
                edit.after.lines[i].size = edit.before.lines[i].size;
                if (edit.before.lines[i].size == 0) {
                    edit.after.lines[i].data = NULL;
                } else {
                    edit.after.lines[i].data =
                        malloc_s((size_t)edit.before.lines[i].size);
                    memcpy(edit.after.lines[i].data, edit.before.lines[i].data,
                           (size_t)edit.before.lines[i].size);
                }
            }

            if (edit.after.size > 1) {
                if (c == ALT_UP) {
                    Str temp = edit.after.lines[0];
                    memmove(&edit.after.lines[0], &edit.after.lines[1],
                            (edit.after.size - 1) * sizeof(Str));
                    edit.after.lines[edit.after.size - 1] = temp;
                } else {
                    Str temp = edit.after.lines[edit.after.size - 1];
                    memmove(&edit.after.lines[1], &edit.after.lines[0],
                            (edit.after.size - 1) * sizeof(Str));
                    edit.after.lines[0] = temp;
                }
            }

            should_set_cursor = true;
            new_cursor = gCurFile->cursor;
            new_cursor.is_selected = gCurFile->cursor.is_selected;
            if (c == ALT_UP) {
                new_cursor.y--;
                new_cursor.select_y--;
            } else {
                new_cursor.y++;
                new_cursor.select_y++;
            }
        } break;

        // Mouse input
        case MOUSE_PRESSED: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;
            int prev_x = curr_x;
            int prev_y = curr_y;
            curr_x = in_x;
            curr_y = in_y;
            int field = getMousePosField(in_x, in_y);
            switch (field) {
                case FIELD_TEXT: {
                    mouse_pressed = FIELD_TEXT;

                    int64_t click_time = getTime();
                    int64_t time_diff = click_time - prev_click_time;

                    if (in_x == prev_x && in_y == prev_y &&
                        time_diff / 1000 < 500) {
                        mouse_click++;
                    } else {
                        mouse_click = 1;
                    }
                    prev_click_time = click_time;

                    gCurFile->bracket_autocomplete = 0;

                    mousePosToEditorPos(&in_x, &in_y);
                    int cx = editorRowRxToCx(&gCurFile->row[in_y], in_x);

                    switch (mouse_click % 4) {
                        case 1:
                            // Mouse to pos
                            gCurFile->cursor.is_selected = false;
                            gCurFile->cursor.y = in_y;
                            gCurFile->cursor.x = cx;
                            gCurFile->sx = in_x;
                            break;
                        case 2: {
                            // Select word
                            const EditorRow* row = &gCurFile->row[in_y];
                            if (row->size == 0)
                                break;
                            if (cx == row->size)
                                cx--;

                            IsCharFunc is_char;
                            if (isSpace(row->data[cx])) {
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
                            editorSelectLine(gCurFile->cursor.y);
                            break;
                        case 0:
                            // Select all
                            should_scroll = false;
                            editorSelectAll();
                            break;
                    }
                } break;

                case FIELD_TOP_STATUS: {
                    should_scroll = false;
                    mouse_click = 0;
                    editorChangeToFile(handleTabClick(in_x));
                } break;

                case FIELD_EXPLORER: {
                    should_scroll = false;
                    mouse_click = 0;
                    if (in_x == gEditor.explorer.width - 1) {
                        mouse_pressed = FIELD_EXPLORER;
                        break;
                    }
                    gEditor.state = EXPLORER_MODE;
                    editorExplorerProcessKeypress(input);
                } break;

                case FIELD_LINENO: {
                    should_scroll = false;
                    mouse_click = 0;
                    int row = gCurFile->row_offset + in_y - 1;
                    if (row < 0)
                        row = 0;
                    if (row >= gCurFile->num_rows)
                        row = gCurFile->num_rows - 1;
                    mouse_pressed = FIELD_LINENO;
                    pressed_row = row;
                    editorSelectLine(row);
                } break;

                default:
                    should_scroll = false;
                    mouse_click = 0;
                    break;
            }
        } break;

        case MOUSE_RELEASED:
            should_scroll = false;
            mouse_pressed = FIELD_EMPTY;
            break;

        case MOUSE_MOVE: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;

            should_scroll = false;
            curr_x = in_x;
            curr_y = in_y;
            if (mouse_pressed == FIELD_EXPLORER) {
                gEditor.explorer.width = gEditor.explorer.prefered_width = in_x;
                if (in_x == 0)
                    gEditor.state = EDIT_MODE;
            } else if (mouse_pressed == FIELD_TEXT) {
                editorMoveMouse(curr_x, curr_y);
            } else if (mouse_pressed == FIELD_LINENO) {
                int col = 0;
                int row = curr_y;
                mousePosToEditorPos(&col, &row);
                editorMoveMouse(0, curr_y + ((row >= pressed_row) ? 1 : 0));
            }
        } break;

        // Scroll up
        case WHEEL_UP: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;
            int field = getMousePosField(in_x, in_y);
            should_scroll = false;
            if (field != FIELD_TEXT && field != FIELD_LINENO) {
                if (field == FIELD_TOP_STATUS) {
                    if (gEditor.tab_offset > 0)
                        gEditor.tab_offset--;
                } else if (field == FIELD_EXPLORER) {
                    editorExplorerProcessKeypress(input);
                }
                break;
            }
        }
        // fall through
        case CTRL_UP:
            should_scroll = false;
            editorScroll(-(c == WHEEL_UP ? 3 : 1));
            if (mouse_pressed == FIELD_TEXT) {
                editorMoveMouse(curr_x, curr_y);
            } else if (mouse_pressed == FIELD_LINENO) {
                int col = 0;
                int row = curr_y;
                mousePosToEditorPos(&col, &row);
                editorMoveMouse(0, curr_y + ((row >= pressed_row) ? 1 : 0));
            }
            break;

        // Scroll down
        case WHEEL_DOWN: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;
            int field = getMousePosField(in_x, in_y);
            should_scroll = false;
            if (field != FIELD_TEXT && field != FIELD_LINENO) {
                if (field == FIELD_TOP_STATUS) {
                    handleTabClick(gEditor.screen_cols);
                } else if (field == FIELD_EXPLORER) {
                    editorExplorerProcessKeypress(input);
                }
                break;
            }
        }
        // fall through
        case CTRL_DOWN:
            should_scroll = false;
            editorScroll(c == WHEEL_DOWN ? 3 : 1);
            if (mouse_pressed == FIELD_TEXT) {
                editorMoveMouse(curr_x, curr_y);
            } else if (mouse_pressed == FIELD_LINENO) {
                int col = 0;
                int row = curr_y;
                mousePosToEditorPos(&col, &row);
                editorMoveMouse(0, curr_y + ((row >= pressed_row) ? 1 : 0));
            }
            break;

        // Close tab
        case SCROLL_PRESSED:
            should_scroll = false;
            break;

        case SCROLL_RELEASED: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;

            should_scroll = false;
            if (getMousePosField(in_x, in_y) == FIELD_TOP_STATUS) {
                int file_index = handleTabClick(in_x);
                if (file_index < 0 || file_index >= gEditor.file_count) {
                    break;
                }

                if (gEditor.files[file_index].dirty) {
                    editorMsg("File has unsaved changes.");
                    editorMsg("Press close again to close file anyway.");
                    editorRefreshScreen();

                    // Read next key to check if it's a repeat
                    EditorInput next_input = editorReadKey();
                    // Have to press in order to release the scroll button
                    if (next_input.type != SCROLL_PRESSED) {
                        pending_input = next_input;
                        break;
                    }

                    next_input = editorReadKey();
                    if (next_input.type != SCROLL_RELEASED ||
                        getMousePosField(next_input.data.cursor.x,
                                         next_input.data.cursor.y) !=
                            FIELD_TOP_STATUS ||
                        handleTabClick(next_input.data.cursor.x) !=
                            file_index) {
                        pending_input = next_input;
                        break;
                    }
                }
                editorCloseFile(file_index);
            }
        } break;

        // Action: Input
        case CHAR_INPUT: {
            c = input.data.unicode;
            has_edit = true;
            EditorSelectRange delete_range = {
                gCurFile->cursor.x, gCurFile->cursor.y, gCurFile->cursor.x,
                gCurFile->cursor.y};

            if (gCurFile->cursor.is_selected) {
                getSelectStartEnd(&gCurFile->cursor, &delete_range);
                editorCopyText(&edit.before, delete_range);
                gCurFile->cursor.is_selected = false;
            }

            edit.x = delete_range.start_x;
            edit.y = delete_range.start_y;
            editorFreeClipboardContent(&edit.after);

            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            bool should_skip = false;
            bool did_autocomplete = false;
            if (c == '\t' && CONVAR_GETINT(whitespace)) {
                int tabsize = CONVAR_GETINT(tabsize);
                int column = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                             gCurFile->cursor.x);
                int total_spaces = tabsize - (column % tabsize);
                if (total_spaces <= 0)
                    total_spaces = tabsize;

                editorClipboardAppendAtRepeat(&edit.after, 0, ' ',
                                              (size_t)total_spaces);
            } else if (!CONVAR_GETINT(bracket)) {
                editorClipboardAppendUnicode(&edit.after, c);
            } else if (close_bracket) {
                editorClipboardAppendUnicode(&edit.after, c);
                editorClipboardAppendChar(&edit.after, close_bracket);
                did_autocomplete = true;
                next_bracket_autocomplete++;
            } else if (open_bracket) {
                if (gCurFile->bracket_autocomplete &&
                    gCurFile->row[gCurFile->cursor.y]
                            .data[gCurFile->cursor.x] == c) {
                    next_bracket_autocomplete--;
                    should_skip = true;
                } else {
                    editorClipboardAppendUnicode(&edit.after, c);
                }
            } else if (c == '\'' || c == '"') {
                if (gCurFile->row[gCurFile->cursor.y]
                        .data[gCurFile->cursor.x] != c) {
                    editorClipboardAppendUnicode(&edit.after, c);
                    editorClipboardAppendChar(&edit.after, c);
                    did_autocomplete = true;
                    next_bracket_autocomplete++;
                } else if (gCurFile->bracket_autocomplete &&
                           gCurFile->row[gCurFile->cursor.y]
                                   .data[gCurFile->cursor.x] == c) {
                    next_bracket_autocomplete--;
                    should_skip = true;
                } else {
                    editorClipboardAppendUnicode(&edit.after, c);
                }
            } else {
                editorClipboardAppendUnicode(&edit.after, c);
            }

            if (next_bracket_autocomplete < 0)
                next_bracket_autocomplete = 0;

            should_set_cursor = true;
            new_cursor = gCurFile->cursor;
            new_cursor.is_selected = false;
            if (should_skip) {
                has_edit = false;
                gCurFile->cursor.x++;
            } else if (did_autocomplete) {
                new_cursor.x = edit.x + edit.after.lines[0].size - 1;
                new_cursor.y = edit.y;
            } else {
                new_cursor.x = edit.x + edit.after.lines[0].size;
                new_cursor.y = edit.y;
            }
        } break;

        default:
            should_scroll = false;
            break;
    }

    if (gCurFile->cursor.x == gCurFile->cursor.select_x &&
        gCurFile->cursor.y == gCurFile->cursor.select_y) {
        gCurFile->cursor.is_selected = false;
    }

    if (!gCurFile->cursor.is_selected) {
        gCurFile->cursor.select_x = gCurFile->cursor.x;
        gCurFile->cursor.select_y = gCurFile->cursor.y;
    }

    if (c != MOUSE_PRESSED && c != MOUSE_RELEASED) {
        mouse_click = 0;
    }

    editorFreeInput(&input);

    if (has_edit) {
        editorApplyEdit(&edit, false);
        if (should_set_cursor) {
            gCurFile->cursor = new_cursor;
            gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                           gCurFile->cursor.x);
        }
        gCurFile->bracket_autocomplete = next_bracket_autocomplete;

        EditorAction* action = calloc_s(1, sizeof(EditorAction));
        action->type = ACTION_EDIT;
        EditAction* edit_action = &action->edit;
        edit_action->data = edit;
        edit_action->old_cursor = old_cursor;
        edit_action->new_cursor = gCurFile->cursor;
        editorAppendAction(action);
    }

    if (should_scroll) {
        editorScrollToCursor();
    }
}
