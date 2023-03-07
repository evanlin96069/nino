#include "editor.h"

#include <stdlib.h>

#include "config.h"
#include "defines.h"
#include "row.h"
#include "terminal.h"
#include "utils.h"

Editor gEditor;
EditorFile* gCurFile;

static EditorFile f;  // temporary placeholder

void editorInit() {
    enableRawMode();
    enableSwap();

    gCurFile = &f;

    gEditor.screen_rows = 0;
    gEditor.screen_cols = 0;

    gEditor.loading = true;
    gEditor.state = EDIT_MODE;
    gEditor.mouse_mode = false;

    gEditor.px = 0;

    gEditor.clipboard.size = 0;
    gEditor.clipboard.data = NULL;

    gEditor.cvars = NULL;

    gEditor.color_cfg = color_default;

    gEditor.status_msg[0][0] = '\0';
    gEditor.status_msg[1][0] = '\0';

    gCurFile->cursor.x = 0;
    gCurFile->cursor.y = 0;
    gCurFile->cursor.is_selected = false;
    gCurFile->cursor.select_x = 0;
    gCurFile->cursor.select_y = 0;

    gCurFile->sx = 0;

    gCurFile->bracket_autocomplete = 0;

    gCurFile->row_offset = 0;
    gCurFile->col_offset = 0;

    gCurFile->num_rows = 0;
    gCurFile->num_rows_digits = 0;

    gCurFile->dirty = 0;
    gCurFile->filename = NULL;

    gCurFile->row = NULL;

    gCurFile->syntax = 0;

    gCurFile->action_head.action = NULL;
    gCurFile->action_head.next = NULL;
    gCurFile->action_head.prev = NULL;
    gCurFile->action_current = &gCurFile->action_head;

    editorInitCommands();
    editorLoadConfig();

    resizeWindow();
    enableAutoResize();

    atexit(terminalExit);
}

void editorFree() {
    // TODO: Multi-file support
    for (int i = 0; i < gCurFile->num_rows; i++) {
        editorFreeRow(&gCurFile->row[i]);
    }
    editorFreeClipboardContent(&gEditor.clipboard);
    editorFreeActionList(gCurFile->action_head.next);
    free(gCurFile->row);
    free(gCurFile->filename);
}

void editorInsertChar(int c) {
    if (gCurFile->cursor.y == gCurFile->num_rows) {
        editorInsertRow(gCurFile->num_rows, "", 0);
    }
    if (c == '\t' && CONVAR_GETINT(whitespace)) {
        int idx = editorRowCxToRx(&(gCurFile->row[gCurFile->cursor.y]),
                                  gCurFile->cursor.x) +
                  1;
        editorInsertChar(' ');
        while (idx % CONVAR_GETINT(tabsize) != 0) {
            editorInsertChar(' ');
            idx++;
        }
    } else {
        editorRowInsertChar(&(gCurFile->row[gCurFile->cursor.y]),
                            gCurFile->cursor.x, c);
        gCurFile->cursor.x++;
    }
}

void editorInsertNewline() {
    int i = 0;

    if (gCurFile->cursor.x == 0) {
        editorInsertRow(gCurFile->cursor.y, "", 0);
    } else {
        editorInsertRow(gCurFile->cursor.y + 1, "", 0);
        EditorRow* curr_row = &(gCurFile->row[gCurFile->cursor.y]);
        EditorRow* new_row = &(gCurFile->row[gCurFile->cursor.y + 1]);
        if (CONVAR_GETINT(autoindent)) {
            while (i < gCurFile->cursor.x &&
                   (curr_row->data[i] == ' ' || curr_row->data[i] == '\t'))
                i++;
            if (i != 0)
                editorRowAppendString(new_row, curr_row->data, i);
            if (curr_row->data[gCurFile->cursor.x - 1] == ':' ||
                (curr_row->data[gCurFile->cursor.x - 1] == '{' &&
                 curr_row->data[gCurFile->cursor.x] != '}')) {
                if (CONVAR_GETINT(whitespace)) {
                    for (int j = 0; j < CONVAR_GETINT(tabsize); j++, i++)
                        editorRowAppendString(new_row, " ", 1);
                } else {
                    editorRowAppendString(new_row, "\t", 1);
                    i++;
                }
            }
        }
        editorRowAppendString(new_row, &(curr_row->data[gCurFile->cursor.x]),
                              curr_row->size - gCurFile->cursor.x);
        curr_row->size = gCurFile->cursor.x;
        curr_row->data[curr_row->size] = '\0';
        editorUpdateRow(curr_row);
    }
    gCurFile->cursor.y++;
    gCurFile->cursor.x = i;
    gCurFile->sx = editorRowCxToRx(&(gCurFile->row[gCurFile->cursor.y]), i);
}

void editorDelChar() {
    if (gCurFile->cursor.y == gCurFile->num_rows)
        return;
    if (gCurFile->cursor.x == 0 && gCurFile->cursor.y == 0)
        return;
    EditorRow* row = &(gCurFile->row[gCurFile->cursor.y]);
    if (gCurFile->cursor.x > 0) {
        editorRowDelChar(row, gCurFile->cursor.x - 1);
        gCurFile->cursor.x--;
    } else {
        gCurFile->cursor.x = gCurFile->row[gCurFile->cursor.y - 1].size;
        editorRowAppendString(&(gCurFile->row[gCurFile->cursor.y - 1]),
                              row->data, row->size);
        editorDelRow(gCurFile->cursor.y);
        gCurFile->cursor.y--;
    }
    gCurFile->sx = editorRowCxToRx(&(gCurFile->row[gCurFile->cursor.y]),
                                   gCurFile->cursor.x);
}
