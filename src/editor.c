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
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.sx = 0;
    E.row_offset = 0;
    E.col_offset = 0;
    E.num_rows = 0;
    E.num_rows_digits = 0;
    E.row = NULL;
    E.is_selected = 0;
    E.select_x = 0;
    E.select_y = 0;
    E.state = EDIT_MODE;
    E.dirty = 0;
    E.bracket_autocomplete = 0;
    E.filename = NULL;
    E.status_msg[0] = '\0';
    E.syntax = 0;

    editorLoadConfig();
    atexit(disableMouse);
    if (E.cfg->mouse)
        enableMouse();

    E.screen_rows = 0;
    E.screen_cols = 0;
    resizeWindow();
    enableAutoResize();
}

void editorFree() {
    for (int i = 0; i < E.num_rows; i++) {
        editorFreeRow(&E.row[i]);
    }
    free(E.row);
    free(E.filename);
}

void editorInsertChar(int c) {
    if (E.cy == E.num_rows) {
        editorInsertRow(E.num_rows, "", 0);
    }
    if (c == '\t' && E.cfg->whitespace) {
        int idx = editorRowCxToRx(&(E.row[E.cy]), E.cx) + 1;
        editorInsertChar(' ');
        while (idx % E.cfg->tab_size != 0) {
            editorInsertChar(' ');
            idx++;
        }
    } else {
        editorRowInsertChar(&(E.row[E.cy]), E.cx, c);
        E.cx++;
    }
}

void editorInsertNewline() {
    int i = 0;

    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        editorInsertRow(E.cy + 1, "", 0);
        EditorRow* curr_row = &(E.row[E.cy]);
        EditorRow* new_row = &(E.row[E.cy + 1]);
        if (E.cfg->auto_indent) {
            while (i < E.cx &&
                   (curr_row->data[i] == ' ' || curr_row->data[i] == '\t'))
                i++;
            if (i != 0)
                editorRowAppendString(new_row, curr_row->data, i);
            if (curr_row->data[E.cx - 1] == ':' ||
                (curr_row->data[E.cx - 1] == '{' &&
                 curr_row->data[E.cx] != '}')) {
                if (E.cfg->whitespace) {
                    for (int j = 0; j < E.cfg->tab_size; j++, i++)
                        editorRowAppendString(new_row, " ", 1);
                } else {
                    editorRowAppendString(new_row, "\t", 1);
                    i++;
                }
            }
        }
        editorRowAppendString(new_row, &(curr_row->data[E.cx]),
                              curr_row->size - E.cx);
        curr_row->size = E.cx;
        curr_row->data[curr_row->size] = '\0';
        editorUpdateRow(curr_row);
    }
    E.cy++;
    E.cx = i;
    E.sx = editorRowCxToRx(&(E.row[E.cy]), i);
}

void editorDelChar() {
    if (E.cy == E.num_rows)
        return;
    if (E.cx == 0 && E.cy == 0)
        return;
    EditorRow* row = &(E.row[E.cy]);
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&(E.row[E.cy - 1]), row->data, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
    E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
}
