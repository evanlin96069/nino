#include "editor.h"

#include <stdlib.h>

#include "config.h"
#include "defines.h"
#include "row.h"
#include "terminal.h"
#include "utils.h"

Editor E;

void editorInit() {
    enableRawMode();
    enableSwap();

    E.cursor.x = 0;
    E.cursor.y = 0;
    E.cursor.is_selected = false;
    E.cursor.select_x = 0;
    E.cursor.select_y = 0;

    E.rx = 0;
    E.sx = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.num_rows_digits = 0;
    E.row = NULL;

    E.state = EDIT_MODE;
    E.mouse_mode = false;

    E.dirty = 0;
    E.bracket_autocomplete = 0;

    E.loading = true;
    E.filename = NULL;
    E.status_msg[0] = '\0';
    E.syntax = 0;

    E.clipboard.size = 0;
    E.clipboard.data = NULL;

    E.color_cfg = color_default;

    E.action_head.action = NULL;
    E.action_head.next = NULL;
    E.action_head.prev = NULL;
    E.action_current = &E.action_head;

    E.cvars = NULL;

    editorInitCommands();
    editorLoadConfig();

    E.screen_rows = 0;
    E.screen_cols = 0;
    resizeWindow();
    enableAutoResize();

    atexit(terminalExit);
}

void editorFree() {
    for (int i = 0; i < E.num_rows; i++) {
        editorFreeRow(&E.row[i]);
    }
    editorFreeClipboardContent(&E.clipboard);
    editorFreeActionList(E.action_head.next);
    free(E.row);
    free(E.filename);
}

void editorInsertChar(int c) {
    if (E.cursor.y == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    if (c == '\t' && CONVAR_GETINT(whitespace)) {
        int idx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x) + 1;
        editorInsertChar(' ');
        while (idx % CONVAR_GETINT(tabsize) != 0) {
            editorInsertChar(' ');
            idx++;
        }
    } else {
        editorRowInsertChar(&(E.row[E.cursor.y]), E.cursor.x, c);
        E.cursor.x++;
    }
}

void editorInsertNewline() {
    int i = 0;

    if (E.cursor.x == 0) {
        editorInsertRow(E.cursor.y, "", 0);
    } else {
        editorInsertRow(E.cursor.y + 1, "", 0);
        EditorRow* curr_row = &(E.row[E.cursor.y]);
        EditorRow* new_row = &(E.row[E.cursor.y + 1]);
        if (CONVAR_GETINT(autoindent)) {
            while (i < E.cursor.x &&
                   (curr_row->data[i] == ' ' || curr_row->data[i] == '\t'))
                i++;
            if (i != 0)
                editorRowAppendString(new_row, curr_row->data, i);
            if (curr_row->data[E.cursor.x - 1] == ':' ||
                (curr_row->data[E.cursor.x - 1] == '{' &&
                 curr_row->data[E.cursor.x] != '}')) {
                if (CONVAR_GETINT(whitespace)) {
                    for (int j = 0; j < CONVAR_GETINT(tabsize); j++, i++)
                        editorRowAppendString(new_row, " ", 1);
                } else {
                    editorRowAppendString(new_row, "\t", 1);
                    i++;
                }
            }
        }
        editorRowAppendString(new_row, &(curr_row->data[E.cursor.x]),
                              curr_row->size - E.cursor.x);
        curr_row->size = E.cursor.x;
        curr_row->data[curr_row->size] = '\0';
        editorUpdateRow(curr_row);
    }
    E.cursor.y++;
    E.cursor.x = i;
    E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), i);
}

void editorDelChar() {
    if (E.cursor.y == E.num_rows)
        return;
    if (E.cursor.x == 0 && E.cursor.y == 0)
        return;
    EditorRow* row = &(E.row[E.cursor.y]);
    if (E.cursor.x > 0) {
        editorRowDelChar(row, E.cursor.x - 1);
        E.cursor.x--;
    } else {
        E.cursor.x = E.row[E.cursor.y - 1].size;
        editorRowAppendString(&(E.row[E.cursor.y - 1]), row->data, row->size);
        editorDelRow(E.cursor.y);
        E.cursor.y--;
    }
    E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
}
