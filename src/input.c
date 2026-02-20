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
    } else {
        OpenStatus result = editorLoadFile(&file, node->filename);
        if (result == OPEN_FILE) {
            editorAddFileToActiveSplit(&file);
        }
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

void editorExplorerScroll(int dist) {
    if (dist == 0)
        return;

    if (dist > 0) {
        // Scroll down
        if ((int)gEditor.explorer.flatten.size - gEditor.explorer.offset >
            gEditor.display_rows) {
            gEditor.explorer.offset += dist;
        }
    } else {
        // Scroll up
        if (gEditor.explorer.offset > 0) {
            gEditor.explorer.offset = (gEditor.explorer.offset + dist) > 0
                                          ? (gEditor.explorer.offset + dist)
                                          : 0;
        }
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
        case WHEEL_DOWN:
            if (editorGetMousePosField(input.data.cursor.x, input.data.cursor.y,
                                       NULL) != FIELD_EXPLORER)
                return false;

            editorExplorerScroll((input.type == WHEEL_UP) ? -3 : 3);
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
            if (editorGetMousePosField(input.data.cursor.x, input.data.cursor.y,
                                       NULL) != FIELD_EXPLORER) {
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
            if (gEditor.split_count != 0) {
                gEditor.state = EDIT_MODE;
            }
            break;
    }
    return true;
}

void editorScrollToCursor(int split_index) {
    EditorTab* tab = editorSplitGetTab(split_index);
    const EditorFile* file = editorTabGetFile(tab);

    int start, end;
    editorGetSplitScreenCols(split_index, &start, &end);
    int cols = (end - start) - editorGetLinenoWidth(file);

    int rx = 0;
    if (tab->cursor.y < file->num_rows) {
        rx = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
    }

    if (tab->cursor.y < tab->row_offset) {
        tab->row_offset = tab->cursor.y;
    }
    if (tab->cursor.y >= tab->row_offset + gEditor.display_rows) {
        tab->row_offset = tab->cursor.y - gEditor.display_rows + 1;
    }
    if (rx < tab->col_offset) {
        tab->col_offset = rx;
    }
    if (rx >= tab->col_offset + cols) {
        tab->col_offset = rx - cols + 1;
    }
}

void editorScrollToCursorCenter(int split_index) {
    EditorTab* tab = editorSplitGetTab(split_index);
    tab->row_offset = tab->cursor.y - gEditor.display_rows / 2;
    if (tab->row_offset < 0) {
        tab->row_offset = 0;
    }
}

int editorGetMousePosField(int x, int y, int* split_index) {
    if (split_index)
        *split_index = 0;

    if (y < 0 || y >= gEditor.screen_rows)
        return FIELD_ERROR;
    if (x < 0 || x >= gEditor.screen_cols)
        return FIELD_ERROR;

    if (y == gEditor.screen_rows - 1)
        return FIELD_STATUS;

    if (y == gEditor.screen_rows - 2 && gEditor.state != EDIT_MODE &&
        gEditor.state != EXPLORER_MODE && gEditor.state != LOADING_MODE)
        return FIELD_PROMPT;

    if (x < gEditor.explorer.width)
        return FIELD_EXPLORER;

    for (int i = 0; i < gEditor.split_count; ++i) {
        int start, end;
        editorGetSplitScreenCols(i, &start, &end);
        if (x < start || x >= end)
            continue;

        if (split_index)
            *split_index = i;

        if (y == 0)
            return FIELD_TOP_STATUS;
        if (x < start + editorGetLinenoWidth(editorGetActiveFile()))
            return FIELD_LINENO;
        return FIELD_TEXT;
    }

    return FIELD_EMPTY;
}

void editorMousePosToEditorPos(int split_index,
                               int mouse_x,
                               int mouse_y,
                               int* out_x,
                               int* out_y) {
    if (!out_x || !out_y)
        return;
    *out_x = 0;
    *out_y = 0;

    if (split_index < 0 || split_index >= gEditor.split_count)
        return;

    int start, end;
    editorGetSplitScreenCols(split_index, &start, &end);
    if (mouse_x < start)
        mouse_x = start;
    if (mouse_x >= end)
        mouse_x = end - 1;

    if (mouse_y < 1)
        mouse_y = 1;
    if (mouse_y >= gEditor.screen_rows - 1)
        mouse_y = gEditor.screen_rows - 2;

    const EditorTab* tab = editorSplitGetTab(split_index);
    const EditorFile* file = editorTabGetFile(tab);

    int row = tab->row_offset + mouse_y - 1;  // offset the top status bar
    if (row < 0)
        return;

    if (row >= file->num_rows) {
        *out_y = file->num_rows - 1;
        *out_x = file->row[*out_y].rsize;
        return;
    }

    int col = mouse_x - start - editorGetLinenoWidth(file) + tab->col_offset;
    if (col < 0) {
        col = 0;
    } else if (col > file->row[row].rsize) {
        col = file->row[row].rsize;
    }

    *out_x = col;
    *out_y = row;
}

void editorScroll(int split_index, int dist) {
    EditorTab* tab = editorSplitGetTab(split_index);
    const EditorFile* file = editorTabGetFile(tab);

    int line = tab->row_offset + dist;
    if (line < 0) {
        line = 0;
    } else if (line >= file->num_rows) {
        line = file->num_rows - 1;
    }
    tab->row_offset = line;
}

static void editorMoveCursor(EditorTab* tab, int key) {
    const EditorFile* file = editorTabGetFile(tab);
    const EditorRow* row = &file->row[tab->cursor.y];

    switch (key) {
        case ARROW_LEFT:
            if (tab->cursor.x != 0) {
                tab->cursor.x = editorRowPreviousUTF8(&file->row[tab->cursor.y],
                                                      tab->cursor.x);
            } else if (tab->cursor.y > 0) {
                tab->cursor.y--;
                tab->cursor.x = file->row[tab->cursor.y].size;
            }
            editorUpdateSx(tab);
            break;

        case ARROW_RIGHT:
            if (row && tab->cursor.x < row->size) {
                tab->cursor.x =
                    editorRowNextUTF8(&file->row[tab->cursor.y], tab->cursor.x);
                editorUpdateSx(tab);
            } else if (row && (tab->cursor.y + 1 < file->num_rows) &&
                       tab->cursor.x == row->size) {
                tab->cursor.y++;
                tab->cursor.x = 0;
                tab->sx = 0;
            }
            break;

        case ARROW_UP:
            if (tab->cursor.y != 0) {
                tab->cursor.y--;
                tab->cursor.x =
                    editorRowRxToCx(&file->row[tab->cursor.y], tab->sx);
            }
            break;

        case ARROW_DOWN:
            if (tab->cursor.y + 1 < file->num_rows) {
                tab->cursor.y++;
                tab->cursor.x =
                    editorRowRxToCx(&file->row[tab->cursor.y], tab->sx);
            }
            break;
    }
    row = (tab->cursor.y >= file->num_rows) ? NULL : &file->row[tab->cursor.y];
    int row_len = row ? row->size : 0;
    if (tab->cursor.x > row_len) {
        tab->cursor.x = row_len;
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

static void editorMoveCursorWordLeft(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);

    if (tab->cursor.x == 0) {
        if (tab->cursor.y == 0)
            return;
        editorMoveCursor(tab, ARROW_LEFT);
    }

    const EditorRow* row = &file->row[tab->cursor.y];
    tab->cursor.x = findPrevCharIndex(row, tab->cursor.x, isIdentifierChar);
    tab->cursor.x = findPrevCharIndex(row, tab->cursor.x, isNonIdentifierChar);
    editorUpdateSx(tab);
}

static void editorMoveCursorWordRight(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);

    if (tab->cursor.x == file->row[tab->cursor.y].size) {
        if (tab->cursor.y == file->num_rows - 1)
            return;
        tab->cursor.x = 0;
        tab->cursor.y++;
    }

    const EditorRow* row = &file->row[tab->cursor.y];
    tab->cursor.x = findNextCharIndex(row, tab->cursor.x, isIdentifierChar);
    tab->cursor.x = findNextCharIndex(row, tab->cursor.x, isNonIdentifierChar);
    editorUpdateSx(tab);
}

static void editorSelectWord(EditorTab* tab,
                             const EditorRow* row,
                             int cx,
                             IsCharFunc is_char) {
    tab->cursor.select_x = findPrevCharIndex(row, cx, is_char);
    tab->cursor.x = findNextCharIndex(row, cx, is_char);
    tab->cursor.is_selected = true;
    editorUpdateSx(tab);
}

static void editorSelectLine(EditorTab* tab, int row) {
    const EditorFile* file = editorTabGetFile(tab);

    if (row < 0 || row >= file->num_rows)
        return;

    tab->cursor.is_selected = true;
    tab->cursor.select_x = 0;
    tab->cursor.select_y = row;
    tab->bracket_autocomplete = 0;

    if (row < file->num_rows - 1) {
        tab->cursor.y = row + 1;
        tab->cursor.x = 0;
    } else {
        tab->cursor.y = row;
        tab->cursor.x = file->row[row].size;
        if (tab->cursor.x == 0) {
            tab->cursor.is_selected = false;
        }
    }

    editorUpdateSx(tab);
}

static void editorSelectAll(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);

    if (file->num_rows == 1 && file->row[0].size == 0)
        return;
    tab->cursor.is_selected = true;
    tab->bracket_autocomplete = 0;
    tab->cursor.y = file->num_rows - 1;
    tab->cursor.x = file->row[file->num_rows - 1].size;
    editorUpdateSx(tab);
    tab->cursor.select_y = 0;
    tab->cursor.select_x = 0;
}

static int editorHandleTabClick(int split_index, int x) {
    if (gEditor.state == LOADING_MODE)
        return -1;

    if (x < gEditor.explorer.width)
        return -1;

    if (split_index < 0 || split_index >= gEditor.split_count)
        return -1;

    int start, end;
    editorGetSplitScreenCols(split_index, &start, &end);
    if (x < start || x >= end)
        return -1;

    EditorSplit* split = &gEditor.splits[split_index];

    bool has_more_files = false;
    int tab_displayed = 0;
    int curr_x = start;
    if (split->tab_offset != 0) {
        if (x == start) {
            split->tab_offset--;
            return -1;
        }
        curr_x++;
    }

    for (int i = split->tab_offset; i < split->tab_count; i++) {
        const EditorFile* file = editorTabGetFile(&split->tabs[i]);

        int tab_width;
        if (file->filename) {
            tab_width = strUTF8Width(getBaseName(file->filename));
        } else {
            // Untitled-%d
            tab_width = strlen("Untitled-") + getDigit(file->new_id + 1);
        }

        // Add * if file is dirty
        if (file->dirty)
            tab_width++;

        // Add padding
        tab_width += 2;

        if (end - curr_x < tab_width ||
            (i != split->tab_count - 1 && end - curr_x == tab_width)) {
            has_more_files = true;
            if (tab_displayed == 0) {
                // Display at least one tab
                tab_width = end - curr_x - 1;
            } else {
                break;
            }
        }

        curr_x += tab_width;
        if (curr_x > x)
            return i;

        tab_displayed++;
    }

    if (has_more_files)
        split->tab_offset++;

    return -1;
}

static void editorMoveMouse(int split_index, int mouse_x, int mouse_y) {
    if (split_index < 0 || split_index >= gEditor.split_count)
        return;

    EditorTab* tab = editorSplitGetTab(split_index);
    const EditorFile* file = editorTabGetFile(tab);

    int x, y;
    editorMousePosToEditorPos(split_index, mouse_x, mouse_y, &x, &y);
    tab->cursor.is_selected = true;
    tab->cursor.x = editorRowRxToCx(&file->row[y], x);
    tab->cursor.y = y;
    tab->sx = x;
}

static void editorCloseTab(int split_index, int tab_index) {
    if (split_index < 0 || split_index >= gEditor.split_count)
        return;

    EditorSplit* split = &gEditor.splits[split_index];
    if (tab_index < 0 || tab_index >= split->tab_count)
        return;

    int old_tab_active_index = split->tab_active_index;
    int old_split_active_index = gEditor.split_active_index;
    editorRemoveTab(split_index, tab_index);
    if (gEditor.split_count == 0) {
        gEditor.state = EXPLORER_MODE;
        editorExplorerShow();
        return;
    }

    if (gEditor.split_active_index != old_split_active_index)
        return;

    if (tab_index < old_tab_active_index) {
        old_tab_active_index--;
    } else if (tab_index == old_tab_active_index &&
               old_tab_active_index >= split->tab_count) {
        old_tab_active_index = split->tab_count - 1;
    }
    editorChangeToFile(split_index, old_tab_active_index);
}

void editorProcessKeypress(void) {
    static int mouse_pressed_field = FIELD_EMPTY;
    static int mouse_pressed_split_index = 0;
    static int mouse_pressed_row = 0;  // For select line drag

    static int64_t prev_click_time = 0;
    static int mouse_click = 0;
    static int curr_x = 0;
    static int curr_y = 0;

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
            EditorFile new_file;
            editorNewUntitledFile(&new_file);
            if (editorAddFileToActiveSplit(&new_file) != -1) {
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

    if (gEditor.split_count == 0) {
        gEditor.state = EXPLORER_MODE;
        editorFreeInput(&input);
        return;
    }

    EditorSplit* split = editorGetActiveSplit();
    EditorTab* tab = editorGetActiveTab();
    EditorFile* file = editorTabGetFile(tab);

    bool should_scroll = true;

    bool has_edit = false;
    Edit edit = {0};
    EditorCursor old_cursor = tab->cursor;
    EditorCursor new_cursor = tab->cursor;

    bool should_set_cursor = false;
    int next_bracket_autocomplete = tab->bracket_autocomplete;

    int c = input.type;
    switch (c) {
        // Action: Newline
        case '\r': {
            has_edit = true;

            edit.x = tab->cursor.x;
            edit.y = tab->cursor.y;

            EditorSelectRange delete_range = {0};
            if (tab->cursor.is_selected) {
                getSelectStartEnd(&tab->cursor, &delete_range);
                editorCopyText(file, &edit.before, delete_range);
            }

            editorFreeClipboardContent(&edit.after);
            editorClipboardAppendNewline(&edit.after);
            editorClipboardAppendNewline(&edit.after);

            if (CONVAR_GETINT(autoindent)) {
                const EditorRow* row = &file->row[tab->cursor.y];
                bool should_indent;
                if (tab->cursor.x < row->size) {
                    should_indent = false;
                    for (int i = tab->cursor.x; i < row->size; i++) {
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
                            if (tab->cursor.x < row->size) {
                                should_inc = (row->data[tab->cursor.x] != '}');
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

            tab->cursor.is_selected = false;
            next_bracket_autocomplete = 0;
            should_set_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.x = edit.after.lines[1].size;
            new_cursor.y = tab->cursor.y + 1;
        } break;

        // Quit editor
        case CTRL_KEY('q'): {
            should_scroll = false;

            int dirty = 0;
            for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
                if (gEditor.files[i].reference_count > 0 &&
                    gEditor.files[i].dirty) {
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

            if (file->reference_count == 1 && file->dirty) {
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
            editorCloseTab(gEditor.split_active_index, split->tab_active_index);
            break;
        }

        // Save
        case CTRL_KEY('s'):
            should_scroll = false;
            if (file->dirty || !file->filename)
                editorSave(file, 0);
            break;

        // Save all
        case ALT_KEY('s'):
            should_scroll = false;
            for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
                if (gEditor.files[i].reference_count > 0 &&
                    (gEditor.files[i].dirty || !gEditor.files[i].filename)) {
                    editorSave(&gEditor.files[i], 0);
                }
            }
            break;

        // Save as
        case ALT_KEY('a'):
            // Alt+A
            should_scroll = false;
            editorSave(file, 1);
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
            int start_x =
                findNextCharIndex(&file->row[tab->cursor.y], 0, isNonSpace);
            if (start_x == tab->cursor.x)
                start_x = 0;
            tab->cursor.x = start_x;
            editorUpdateSx(tab);
            tab->cursor.is_selected = (c == (SHIFT_HOME));
            tab->bracket_autocomplete = 0;
        } break;

        case END_KEY:
        case SHIFT_END:
            if (tab->cursor.y < file->num_rows &&
                tab->cursor.x != file->row[tab->cursor.y].size) {
                tab->cursor.x = file->row[tab->cursor.y].size;
                editorUpdateSx(tab);
                tab->cursor.is_selected = (c == SHIFT_END);
                tab->bracket_autocomplete = 0;
            }
            break;

        case CTRL_LEFT:
        case SHIFT_CTRL_LEFT:
            editorMoveCursorWordLeft(tab);
            tab->cursor.is_selected = (c == SHIFT_CTRL_LEFT);
            tab->bracket_autocomplete = 0;
            break;

        case CTRL_RIGHT:
        case SHIFT_CTRL_RIGHT:
            editorMoveCursorWordRight(tab);
            tab->cursor.is_selected = (c == SHIFT_CTRL_RIGHT);
            tab->bracket_autocomplete = 0;
            break;

        // Find
        case CTRL_KEY('f'):
            should_scroll = false;
            tab->cursor.is_selected = false;
            tab->bracket_autocomplete = 0;
            editorFind();
            break;

        // Goto line
        case CTRL_KEY('g'):
            should_scroll = false;
            tab->cursor.is_selected = false;
            tab->bracket_autocomplete = 0;
            editorGotoLine();
            break;

        // Select line
        case CTRL_KEY('l'):
            editorSelectLine(tab, tab->cursor.y);
            break;

        // Select all
        case CTRL_KEY('a'):
            should_scroll = false;
            editorSelectAll(tab);
            break;

        // Action: Delete
        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE: {
            if (!tab->cursor.is_selected) {
                if (c == DEL_KEY) {
                    if (tab->cursor.y == file->num_rows - 1 &&
                        tab->cursor.x == file->row[file->num_rows - 1].size)
                        break;
                } else if (tab->cursor.x == 0 && tab->cursor.y == 0) {
                    break;
                }
            }

            has_edit = true;

            if (tab->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&tab->cursor, &range);
                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(file, &edit.before, range);
                editorFreeClipboardContent(&edit.after);
                should_set_cursor = true;
                new_cursor = tab->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
                tab->cursor.is_selected = false;
                break;
            }

            int cx = tab->cursor.x;
            int cy = tab->cursor.y;
            int start_x = cx;
            int start_y = cy;
            int end_x = cx;
            int end_y = cy;
            int new_x = cx;
            int new_y = cy;
            int bracket_delta = 0;

            EditorRow* row = &file->row[cy];

            if (c == DEL_KEY) {
                if (cx < row->size) {
                    end_x = editorRowNextUTF8(row, cx);
                } else if (cy + 1 < file->num_rows) {
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
                    start_x = file->row[start_y].size;
                    new_y = start_y;
                    new_x = start_x;
                }
            }

            if (tab->bracket_autocomplete && cx > 0 && cx < row->size) {
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
            editorCopyText(file, &edit.before, range);
            editorFreeClipboardContent(&edit.after);
            next_bracket_autocomplete += bracket_delta;
            if (next_bracket_autocomplete < 0)
                next_bracket_autocomplete = 0;
            should_set_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            new_cursor.x = new_x;
            new_cursor.y = new_y;
        } break;

        // Action: Cut
        case CTRL_KEY('x'): {
            if (file->num_rows == 1 && file->row[0].size == 0)
                break;

            has_edit = true;
            editorFreeClipboardContent(&gEditor.clipboard);

            if (!tab->cursor.is_selected) {
                // Copy line
                editorCopyLine(file, &gEditor.clipboard, tab->cursor.y);
                gEditor.copy_line = true;

                // Delete line
                EditorSelectRange range = {0, tab->cursor.y,
                                           file->row[tab->cursor.y].size,
                                           tab->cursor.y};
                if (file->num_rows != 1) {
                    if (tab->cursor.y == file->num_rows - 1) {
                        range.start_y--;
                        range.start_x = file->row[range.start_y].size;
                    } else {
                        range.end_y++;
                        range.end_x = 0;
                    }
                }

                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(file, &edit.before, range);
                editorFreeClipboardContent(&edit.after);
                should_set_cursor = true;
                new_cursor = tab->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
            } else {
                EditorSelectRange range;
                getSelectStartEnd(&tab->cursor, &range);
                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(file, &edit.before, range);
                editorFreeClipboardContent(&edit.after);
                editorCopyText(file, &gEditor.clipboard, range);
                gEditor.copy_line = false;
                should_set_cursor = true;
                new_cursor = tab->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
            }
            editorCopyToSysClipboard(&gEditor.clipboard, file->newline);
        } break;

        // Copy
        case CTRL_KEY('c'): {
            editorFreeClipboardContent(&gEditor.clipboard);
            should_scroll = false;

            if (tab->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&tab->cursor, &range);
                gEditor.copy_line = false;
                editorCopyText(file, &gEditor.clipboard, range);
            } else {
                editorCopyLine(file, &gEditor.clipboard, tab->cursor.y);
                gEditor.copy_line = true;
            }
            editorCopyToSysClipboard(&gEditor.clipboard, file->newline);
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
                tab->cursor.x,
                tab->cursor.y,
                tab->cursor.x,
                tab->cursor.y,
            };
            if (tab->cursor.is_selected) {
                getSelectStartEnd(&tab->cursor, &delete_range);
                editorCopyText(file, &edit.before, delete_range);
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

            if (!tab->cursor.is_selected && copy_line) {
                edit.x = 0;
            }

            should_set_cursor = true;
            new_cursor = tab->cursor;
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

            tab->cursor.is_selected = false;
        } break;

        // Undo
        case CTRL_KEY('z'):
            tab->cursor.is_selected = false;
            tab->bracket_autocomplete = 0;
            should_scroll = editorUndo(tab);
            break;

        // Redo
        case CTRL_KEY('y'):
            tab->cursor.is_selected = false;
            tab->bracket_autocomplete = 0;
            should_scroll = editorRedo(tab);
            break;

        // Select word
        case CTRL_KEY('d'): {
            const EditorRow* row = &file->row[tab->cursor.y];
            if (tab->cursor.x < row->size &&
                !isIdentifierChar(row->data[tab->cursor.x])) {
                should_scroll = false;
                break;
            }
            editorSelectWord(tab, row, tab->cursor.x, isNonIdentifierChar);
        } break;

        // Previous file
        case CTRL_KEY('['):
            should_scroll = false;
            if (split->tab_count < 2)
                break;

            if (split->tab_active_index == 0) {
                editorChangeToFile(gEditor.split_active_index,
                                   split->tab_count - 1);
            } else {
                editorChangeToFile(gEditor.split_active_index,
                                   split->tab_active_index - 1);
            }
            break;

        // Next file
        case CTRL_KEY(']'):
            should_scroll = false;
            if (split->tab_count < 2)
                break;

            if (split->tab_active_index == split->tab_count - 1) {
                editorChangeToFile(gEditor.split_active_index, 0);
            } else {
                editorChangeToFile(gEditor.split_active_index,
                                   split->tab_active_index + 1);
            }
            break;

        case SHIFT_PAGE_UP:
        case SHIFT_PAGE_DOWN:
        case PAGE_UP:
        case PAGE_DOWN: {
            tab->cursor.is_selected =
                (c == SHIFT_PAGE_UP || c == SHIFT_PAGE_DOWN);
            tab->bracket_autocomplete = 0;

            if (c == PAGE_UP || c == SHIFT_PAGE_UP) {
                tab->cursor.y = tab->row_offset;
            } else if (c == PAGE_DOWN || c == SHIFT_PAGE_DOWN) {
                tab->cursor.y = tab->row_offset + gEditor.display_rows - 1;
                if (tab->cursor.y >= file->num_rows)
                    tab->cursor.y = file->num_rows - 1;
            }

            int times = gEditor.display_rows;
            while (times--) {
                if (c == PAGE_UP || c == SHIFT_PAGE_UP) {
                    if (tab->cursor.y == 0) {
                        tab->cursor.x = 0;
                        tab->sx = 0;
                        break;
                    }
                    editorMoveCursor(tab, ARROW_UP);
                } else {
                    if (tab->cursor.y == file->num_rows - 1) {
                        tab->cursor.x = file->row[tab->cursor.y].size;
                        break;
                    }
                    editorMoveCursor(tab, ARROW_DOWN);
                }
            }
        } break;

        case SHIFT_CTRL_PAGE_UP:
        case CTRL_PAGE_UP:
            tab->cursor.is_selected = (c == SHIFT_CTRL_PAGE_UP);
            tab->bracket_autocomplete = 0;
            while (tab->cursor.y > 0) {
                editorMoveCursor(tab, ARROW_UP);
                if (file->row[tab->cursor.y].size == 0) {
                    break;
                }
            }
            break;

        case SHIFT_CTRL_PAGE_DOWN:
        case CTRL_PAGE_DOWN:
            tab->cursor.is_selected = (c == SHIFT_CTRL_PAGE_DOWN);
            tab->bracket_autocomplete = 0;
            while (tab->cursor.y < file->num_rows - 1) {
                editorMoveCursor(tab, ARROW_DOWN);
                if (file->row[tab->cursor.y].size == 0) {
                    break;
                }
            }
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            if (tab->cursor.is_selected) {
                EditorSelectRange range;
                getSelectStartEnd(&tab->cursor, &range);

                if (c == ARROW_UP || c == ARROW_LEFT) {
                    tab->cursor.x = range.start_x;
                    tab->cursor.y = range.start_y;
                } else {
                    tab->cursor.x = range.end_x;
                    tab->cursor.y = range.end_y;
                }
                editorUpdateSx(tab);
                if (c == ARROW_UP || c == ARROW_DOWN) {
                    editorMoveCursor(tab, c);
                }
                tab->cursor.is_selected = false;
            } else {
                if (tab->bracket_autocomplete) {
                    if (c == ARROW_RIGHT) {
                        tab->bracket_autocomplete--;
                    } else {
                        tab->bracket_autocomplete = 0;
                    }
                }
                editorMoveCursor(tab, c);
            }
            break;

        case SHIFT_UP:
        case SHIFT_DOWN:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            tab->cursor.is_selected = true;
            tab->bracket_autocomplete = 0;
            editorMoveCursor(tab, c - (SHIFT_UP - ARROW_UP));
            break;

        case CTRL_HOME:
            tab->cursor.is_selected = false;
            tab->bracket_autocomplete = 0;
            tab->cursor.y = 0;
            tab->cursor.x = 0;
            tab->sx = 0;
            break;

        case CTRL_END:
            tab->cursor.is_selected = false;
            tab->bracket_autocomplete = 0;
            tab->cursor.y = file->num_rows - 1;
            tab->cursor.x = file->row[file->num_rows - 1].size;
            editorUpdateSx(tab);
            break;

        // Action: Copy Line Up
        // Action: Copy Line Down
        case SHIFT_ALT_UP:
        case SHIFT_ALT_DOWN:
            has_edit = true;
            tab->cursor.is_selected = false;
            edit.x = 0;
            edit.y = tab->cursor.y;
            editorCopyLine(file, &edit.after, tab->cursor.y);
            should_set_cursor = true;
            new_cursor = tab->cursor;
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
            getSelectStartEnd(&tab->cursor, &range);
            if (c == ALT_UP) {
                if (range.start_y == 0)
                    break;
            } else {
                if (range.end_y == file->num_rows - 1)
                    break;
            }

            has_edit = true;

            edit.x = 0;
            edit.y = (c == ALT_UP) ? range.start_y - 1 : range.start_y;
            range.start_x = 0;
            if (c == ALT_UP) {
                range.start_y--;
                range.end_x = file->row[range.end_y].size;
            } else {
                range.end_y++;
                range.end_x = file->row[range.end_y].size;
            }
            editorCopyText(file, &edit.before, range);
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
            new_cursor = tab->cursor;
            new_cursor.is_selected = tab->cursor.is_selected;
            if (c == ALT_UP) {
                new_cursor.y--;
                new_cursor.select_y--;
            } else {
                new_cursor.y++;
                new_cursor.select_y++;
            }
        } break;

        // Create new split
        case CTRL_KEY('\\'): {
            should_scroll = false;
            int new_split_index = editorAddSplit();
            if (new_split_index != -1) {
                gEditor.split_active_index = new_split_index;
                editorAddTab(new_split_index, tab->file_index);
            }
        } break;

        // Focus left/right split
        case CTRL_ALT_RIGHT:
            should_scroll = false;
            if (gEditor.split_active_index < gEditor.split_count - 1) {
                gEditor.split_active_index++;
            }
            break;

        case CTRL_ALT_LEFT:
            should_scroll = false;
            if (gEditor.split_active_index > 0) {
                gEditor.split_active_index--;
            }
            break;

        // Mouse input
        case MOUSE_PRESSED: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;
            int prev_x = curr_x;
            int prev_y = curr_y;
            curr_x = in_x;
            curr_y = in_y;

            int split_index;
            int field = editorGetMousePosField(in_x, in_y, &split_index);

            switch (field) {
                case FIELD_TEXT: {
                    gEditor.split_active_index = split_index;

                    mouse_pressed_field = FIELD_TEXT;
                    mouse_pressed_split_index = split_index;

                    EditorTab* cursor_tab = editorSplitGetTab(split_index);
                    EditorFile* cursor_file = editorTabGetFile(cursor_tab);

                    int64_t click_time = getTime();
                    int64_t time_diff = click_time - prev_click_time;

                    if (in_x == prev_x && in_y == prev_y &&
                        time_diff / 1000 < 500) {
                        mouse_click++;
                    } else {
                        mouse_click = 1;
                    }
                    prev_click_time = click_time;

                    tab->bracket_autocomplete = 0;

                    int x, y;
                    editorMousePosToEditorPos(split_index, in_x, in_y, &x, &y);
                    int cx = editorRowRxToCx(&cursor_file->row[y], x);

                    switch (mouse_click % 4) {
                        case 1:
                            // Mouse to pos
                            cursor_tab->cursor.is_selected = false;
                            cursor_tab->cursor.y = y;
                            cursor_tab->cursor.x = cx;
                            cursor_tab->sx = x;
                            break;
                        case 2: {
                            // Select word
                            const EditorRow* row = &cursor_file->row[y];
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
                            editorSelectWord(cursor_tab, row, cx, is_char);
                        } break;
                        case 3:
                            // Select line
                            editorSelectLine(cursor_tab, tab->cursor.y);
                            break;
                        case 0:
                            // Select all
                            should_scroll = false;
                            editorSelectAll(cursor_tab);
                            break;
                    }
                } break;

                case FIELD_TOP_STATUS: {
                    gEditor.split_active_index = split_index;
                    should_scroll = false;
                    mouse_click = 0;
                    editorChangeToFile(split_index,
                                       editorHandleTabClick(split_index, in_x));
                } break;

                case FIELD_LINENO: {
                    gEditor.split_active_index = split_index;
                    should_scroll = false;
                    mouse_click = 0;

                    EditorTab* cursor_tab = editorSplitGetTab(split_index);
                    EditorFile* cursor_file = editorTabGetFile(cursor_tab);

                    int row = cursor_tab->row_offset + in_y - 1;
                    if (row < 0)
                        row = 0;
                    if (row >= cursor_file->num_rows)
                        row = cursor_file->num_rows - 1;
                    mouse_pressed_field = FIELD_LINENO;
                    mouse_pressed_split_index = split_index;
                    mouse_pressed_row = row;
                    editorSelectLine(cursor_tab, row);
                } break;

                case FIELD_EXPLORER: {
                    should_scroll = false;
                    mouse_click = 0;
                    if (in_x == gEditor.explorer.width - 1) {
                        mouse_pressed_field = FIELD_EXPLORER;
                        break;
                    }
                    gEditor.state = EXPLORER_MODE;
                    editorExplorerProcessKeypress(input);
                } break;

                default:
                    should_scroll = false;
                    mouse_click = 0;
                    break;
            }
        } break;

        case MOUSE_RELEASED:
            should_scroll = false;
            mouse_pressed_field = FIELD_EMPTY;
            break;

        case MOUSE_MOVE: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;

            should_scroll = false;
            curr_x = in_x;
            curr_y = in_y;
            if (mouse_pressed_field == FIELD_EXPLORER) {
                gEditor.explorer.width = gEditor.explorer.prefered_width = in_x;
                if (in_x == 0)
                    gEditor.state = EDIT_MODE;
            } else if (mouse_pressed_field == FIELD_TEXT) {
                editorMoveMouse(mouse_pressed_split_index, curr_x, curr_y);
            } else if (mouse_pressed_field == FIELD_LINENO) {
                int x, y;
                editorMousePosToEditorPos(mouse_pressed_split_index, 0, curr_y,
                                          &x, &y);
                editorMoveMouse(mouse_pressed_split_index, 0,
                                curr_y + ((y >= mouse_pressed_row) ? 1 : 0));
            }
        } break;

        // Scroll
        case WHEEL_UP:
        case WHEEL_DOWN:
        case CTRL_UP:
        case CTRL_DOWN: {
            should_scroll = false;
            int scroll_dist = (c == WHEEL_UP || c == WHEEL_DOWN) ? 3 : 1;
            int scroll_dir = (c == WHEEL_UP || c == CTRL_UP) ? -1 : 1;

            int scroll_split_index = gEditor.split_active_index;

            if (c == WHEEL_UP || c == WHEEL_DOWN) {
                int in_x = input.data.cursor.x;
                int in_y = input.data.cursor.y;

                bool should_break = false;
                int split_index;
                int field = editorGetMousePosField(in_x, in_y, &split_index);
                switch (field) {
                    case FIELD_TEXT:
                    case FIELD_LINENO:
                        scroll_split_index = split_index;
                        break;
                    case FIELD_TOP_STATUS: {
                        EditorSplit* scroll_split =
                            &gEditor.splits[split_index];
                        if (c == WHEEL_UP) {
                            if (scroll_split->tab_offset > 0) {
                                scroll_split->tab_offset--;
                            }
                        } else {
                            if (scroll_split->tab_offset +
                                    scroll_split->tab_displayed <
                                scroll_split->tab_count) {
                                scroll_split->tab_offset++;
                            }
                        }
                        should_break = true;
                    } break;
                    case FIELD_EXPLORER:
                        editorExplorerProcessKeypress(input);
                        should_break = true;
                    default:
                        break;
                }

                if (should_break)
                    break;
            }

            editorScroll(scroll_split_index, scroll_dir * scroll_dist);
            if (scroll_split_index == mouse_pressed_split_index) {
                if (mouse_pressed_field == FIELD_TEXT) {
                    editorMoveMouse(mouse_pressed_split_index, curr_x, curr_y);
                } else if (mouse_pressed_field == FIELD_LINENO) {
                    int x, y;
                    editorMousePosToEditorPos(mouse_pressed_split_index, 0,
                                              curr_y, &x, &y);
                    editorMoveMouse(
                        mouse_pressed_split_index, 0,
                        curr_y + ((y >= mouse_pressed_row) ? 1 : 0));
                }
            }
        } break;

        // Close tab
        case SCROLL_PRESSED:
            should_scroll = false;
            break;

        case SCROLL_RELEASED: {
            int in_x = input.data.cursor.x;
            int in_y = input.data.cursor.y;
            should_scroll = false;

            int split_index;
            if (editorGetMousePosField(in_x, in_y, &split_index) ==
                FIELD_TOP_STATUS) {
                EditorSplit* target_split = &gEditor.splits[split_index];
                int tab_index = editorHandleTabClick(split_index, in_x);
                if (tab_index < 0 || tab_index >= target_split->tab_count)
                    break;

                const EditorTab* target_tab =
                    &gEditor.splits[split_index].tabs[tab_index];
                const EditorFile* tab_file = editorTabGetFile(target_tab);
                if (tab_file->reference_count == 1 && tab_file->dirty) {
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
                    int next_split_index;
                    if (next_input.type == SCROLL_RELEASED &&
                        editorGetMousePosField(
                            next_input.data.cursor.x, next_input.data.cursor.y,
                            &next_split_index) == FIELD_TOP_STATUS) {
                        if (next_split_index == split_index &&
                            editorHandleTabClick(split_index,
                                                 next_input.data.cursor.x) ==
                                tab_index) {
                            editorCloseTab(split_index, tab_index);
                            break;
                        }
                    }
                    pending_input = next_input;
                } else {
                    editorCloseTab(split_index, tab_index);
                }
            }
        } break;

        // Action: Input
        case CHAR_INPUT: {
            c = input.data.unicode;
            has_edit = true;
            EditorSelectRange delete_range = {
                tab->cursor.x,
                tab->cursor.y,
                tab->cursor.x,
                tab->cursor.y,
            };

            if (tab->cursor.is_selected) {
                getSelectStartEnd(&tab->cursor, &delete_range);
                editorCopyText(file, &edit.before, delete_range);
                tab->cursor.is_selected = false;
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
                int column =
                    editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
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
                if (tab->bracket_autocomplete &&
                    file->row[tab->cursor.y].data[tab->cursor.x] == c) {
                    next_bracket_autocomplete--;
                    should_skip = true;
                } else {
                    editorClipboardAppendUnicode(&edit.after, c);
                }
            } else if (c == '\'' || c == '"') {
                if (file->row[tab->cursor.y].data[tab->cursor.x] != c) {
                    editorClipboardAppendUnicode(&edit.after, c);
                    editorClipboardAppendChar(&edit.after, c);
                    did_autocomplete = true;
                    next_bracket_autocomplete++;
                } else if (tab->bracket_autocomplete &&
                           file->row[tab->cursor.y].data[tab->cursor.x] == c) {
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
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            if (should_skip) {
                has_edit = false;
                tab->cursor.x++;
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

    if (tab->cursor.x == tab->cursor.select_x &&
        tab->cursor.y == tab->cursor.select_y) {
        tab->cursor.is_selected = false;
    }

    if (!tab->cursor.is_selected) {
        tab->cursor.select_x = tab->cursor.x;
        tab->cursor.select_y = tab->cursor.y;
    }

    if (c != MOUSE_PRESSED && c != MOUSE_RELEASED) {
        mouse_click = 0;
    }

    editorFreeInput(&input);

    if (has_edit) {
        editorApplyEdit(tab, &edit, false);
        if (should_set_cursor) {
            tab->cursor = new_cursor;
            editorUpdateSx(tab);
        }
        tab->bracket_autocomplete = next_bracket_autocomplete;

        EditorAction* action = calloc_s(1, sizeof(EditorAction));
        action->type = ACTION_EDIT;
        EditAction* edit_action = &action->edit;
        edit_action->data = edit;
        edit_action->old_cursor = old_cursor;
        edit_action->new_cursor = tab->cursor;
        editorAppendAction(file, action);
    }

    if (should_scroll) {
        editorScrollToCursor(gEditor.split_active_index);
    }
}
