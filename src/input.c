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
    } else if (editorOpen(&file, node->filename)) {
        int index = editorAddFile(&file);
        if (index == -1) {
            editorFreeFile(&file);
        }
        // hack: refresh screen to update gEditor.tab_displayed
        editorRefreshScreen();
        editorChangeToFile(index);
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

static inline void editorExplorerShow(void) {
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
    int cols =
        gEditor.screen_cols - gEditor.explorer.width - gCurFile->lineno_width;
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
    if (x < gEditor.explorer.width + gCurFile->lineno_width)
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

    int col = *x - gEditor.explorer.width - gCurFile->lineno_width +
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
        const char* filename =
            file->filename ? getBaseName(file->filename) : "Untitled";
        int tab_width = strUTF8Width(filename) + 2;

        if (file->dirty)
            tab_width++;

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

// Protect closing file with unsaved changes
static int close_protect = -1;
static void editorCloseFile(int index) {
    if (index < 0 || index > gEditor.file_count) {
        close_protect = -1;
        return;
    }

    if (gEditor.files[index].dirty && close_protect != index) {
        editorMsg(
            "File has unsaved changes. Press again to close file "
            "anyway.");
        close_protect = index;
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
    // Protect quiting program with unsaved files
    static bool quit_protect = true;

    static bool pressed = false;
    static int64_t prev_click_time = 0;
    static int mouse_click = 0;
    static int curr_x = 0;
    static int curr_y = 0;
    static bool pressed_explorer = false;

    // Only the paste input need to be free, so we skipped some cases
    EditorInput input = editorReadKey();

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
    }

    editorMsgClear();

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

    bool should_record_action = false;

    EditorAction* action = calloc_s(1, sizeof(EditorAction));
    action->type = ACTION_EDIT;
    EditAction* edit = &action->edit;

    edit->old_cursor = gCurFile->cursor;

    int c = input.type;
    int x = input.data.cursor.x;
    int y = input.data.cursor.y;
    switch (c) {
        // Action: Newline
        case '\r': {
            should_record_action = true;

            getSelectStartEnd(&edit->deleted_range);

            if (gCurFile->cursor.is_selected) {
                editorCopyText(&edit->deleted_text, edit->deleted_range);
                editorDeleteText(edit->deleted_range);
                gCurFile->cursor.is_selected = false;
            }

            gCurFile->bracket_autocomplete = 0;

            edit->added_range.start_x = gCurFile->cursor.x;
            edit->added_range.start_y = gCurFile->cursor.y;
            editorInsertNewline();
            edit->added_range.end_x = gCurFile->cursor.x;
            edit->added_range.end_y = gCurFile->cursor.y;
            editorCopyText(&edit->added_text, edit->added_range);
        } break;

        // Quit editor
        case CTRL_KEY('q'): {
            close_protect = -1;
            editorFreeAction(action);
            bool dirty = false;
            for (int i = 0; i < gEditor.file_count; i++) {
                if (gEditor.files[i].dirty) {
                    dirty = true;
                    break;
                }
            }
            if (dirty && quit_protect) {
                editorMsg(
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
            editorCloseFile(gEditor.file_index);
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
                getSelectStartEnd(&edit->deleted_range);
                editorCopyText(&edit->deleted_text, edit->deleted_range);
                editorDeleteText(edit->deleted_range);
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

            edit->deleted_range.end_x = gCurFile->cursor.x;
            edit->deleted_range.end_y = gCurFile->cursor.y;

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
                    if (!isSpace(gCurFile->row[gCurFile->cursor.y].data[i])) {
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
            edit->deleted_range.start_x = gCurFile->cursor.x;
            edit->deleted_range.start_y = gCurFile->cursor.y;
            editorCopyText(&edit->deleted_text, edit->deleted_range);
            editorDeleteText(edit->deleted_range);
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

                edit->deleted_range = range;
                editorCopyText(&edit->deleted_text, edit->deleted_range);
                editorDeleteText(edit->deleted_range);
            } else {
                getSelectStartEnd(&edit->deleted_range);
                editorCopyText(&edit->deleted_text, edit->deleted_range);
                editorCopyText(&gEditor.clipboard, edit->deleted_range);
                editorDeleteText(edit->deleted_range);
                gCurFile->cursor.is_selected = false;
            }
            editorCopyToSysClipboard(&gEditor.clipboard);
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
            editorCopyToSysClipboard(&gEditor.clipboard);
        } break;

        // Action: Paste
        case PASTE_INPUT:
        case CTRL_KEY('v'): {
            EditorClipboard* clipboard =
                (c == PASTE_INPUT) ? &input.data.paste : &gEditor.clipboard;

            if (!clipboard->size)
                break;

            should_record_action = true;

            getSelectStartEnd(&edit->deleted_range);

            if (gCurFile->cursor.is_selected) {
                editorCopyText(&edit->deleted_text, edit->deleted_range);
                editorDeleteText(edit->deleted_range);
                gCurFile->cursor.is_selected = false;
            }

            edit->added_range.start_x = gCurFile->cursor.x;
            edit->added_range.start_y = gCurFile->cursor.y;
            editorPasteText(clipboard, gCurFile->cursor.x, gCurFile->cursor.y);

            edit->added_range.end_x = gCurFile->cursor.x;
            edit->added_range.end_y = gCurFile->cursor.y;
            editorCopyText(&edit->added_text, edit->added_range);
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
            should_record_action = true;
            gCurFile->cursor.is_selected = false;
            edit->old_cursor.is_selected = 0;
            editorInsertRow(gCurFile, gCurFile->cursor.y,
                            gCurFile->row[gCurFile->cursor.y].data,
                            gCurFile->row[gCurFile->cursor.y].size);

            edit->added_range.start_x = gCurFile->row[gCurFile->cursor.y].size;
            edit->added_range.start_y = gCurFile->cursor.y;
            edit->added_range.end_x =
                gCurFile->row[gCurFile->cursor.y + 1].size;
            edit->added_range.end_y = gCurFile->cursor.y + 1;
            editorCopyText(&edit->added_text, edit->added_range);

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
                editorCopyText(&edit->added_text, range);
                //  Move empty string at the start to the end
                Str temp = edit->added_text.lines[0];
                memmove(&edit->added_text.lines[0], &edit->added_text.lines[1],
                        (edit->added_text.size - 1) * sizeof(Str));
                edit->added_text.lines[edit->added_text.size - 1] = temp;
            } else {
                range.end_x = 0;
                range.end_y++;
                editorCopyText(&edit->added_text, range);
                // Move empty string at the end to the start
                Str temp = edit->added_text.lines[edit->added_text.size - 1];
                memmove(&edit->added_text.lines[1], &edit->added_text.lines[0],
                        (edit->added_text.size - 1) * sizeof(Str));
                edit->added_text.lines[0] = temp;
            }
            edit->deleted_range = range;
            editorCopyText(&edit->deleted_text, range);
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
            editorPasteText(&edit->added_text, paste_x, gCurFile->cursor.y);
            range.end_x = gCurFile->cursor.x;
            range.end_y = gCurFile->cursor.y;
            edit->added_range = range;

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
                    if (x == gEditor.explorer.width - 1) {
                        pressed_explorer = true;
                        break;
                    }
                    gEditor.state = EXPLORER_MODE;
                    editorExplorerProcessKeypress(input);
                }
                break;
            }

            int64_t click_time = getTime();
            int64_t time_diff = click_time - prev_click_time;

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
            pressed_explorer = false;
            break;

        case MOUSE_MOVE:
            should_scroll = false;
            if (pressed_explorer) {
                gEditor.explorer.width = gEditor.explorer.prefered_width = x;
                if (x == 0)
                    gEditor.state = EDIT_MODE;
            } else if (moveMouse(x, y)) {
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
                    editorExplorerProcessKeypress(input);
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
            if (pressed)
                moveMouse(curr_x, curr_y);
            break;

        // Close tab
        case SCROLL_PRESSED:
            // Return to prevent resetting close_protect
            editorFreeAction(action);
            return;

        case SCROLL_RELEASED:
            should_scroll = false;
            if (getMousePosField(x, y) == FIELD_TOP_STATUS) {
                quit_protect = true;
                editorFreeAction(action);
                editorCloseFile(handleTabClick(x));
                return;
            }
            break;

        // Action: Input
        case CHAR_INPUT: {
            c = input.data.unicode;
            should_record_action = true;

            getSelectStartEnd(&edit->deleted_range);

            if (gCurFile->cursor.is_selected) {
                editorCopyText(&edit->deleted_text, edit->deleted_range);
                editorDeleteText(edit->deleted_range);
                gCurFile->cursor.is_selected = false;
            }

            int x_offset = 0;
            edit->added_range.start_x = gCurFile->cursor.x;
            edit->added_range.start_y = gCurFile->cursor.y;

            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            if (!CONVAR_GETINT(bracket)) {
                editorInsertUnicode(c);
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
                editorInsertUnicode(c);
            }

            edit->added_range.end_x = gCurFile->cursor.x + x_offset;
            edit->added_range.end_y = gCurFile->cursor.y;
            editorCopyText(&edit->added_text, edit->added_range);

            gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                           gCurFile->cursor.x);
            gCurFile->cursor.is_selected = false;

            if (x_offset == -1) {
                should_record_action = false;
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

    if (c != MOUSE_PRESSED && c != MOUSE_RELEASED)
        mouse_click = 0;

    editorFreeInput(&input);

    if (should_record_action) {
        edit->new_cursor = gCurFile->cursor;
        editorAppendAction(action);
    } else {
        editorFreeAction(action);
    }

    if (should_scroll)
        editorScrollToCursor();
    close_protect = -1;
    quit_protect = true;
}
