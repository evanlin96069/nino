#include "select.h"

#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "row.h"
#include "utils.h"

void getSelectStartEnd(int* start_x, int* start_y, int* end_x, int* end_y) {
    if (E.select_y > E.cy) {
        *start_x = E.cx;
        *start_y = E.cy;
        *end_x = E.select_x;
        *end_y = E.select_y;
    } else if (E.select_y < E.cy) {
        *start_x = E.select_x;
        *start_y = E.select_y;
        *end_x = E.cx;
        *end_y = E.cy;
    } else {
        // same row
        *start_y = *end_y = E.cy;
        *start_x = E.select_x > E.cx ? E.cx : E.select_x;
        *end_x = E.select_x > E.cx ? E.select_x : E.cx;
    }
}

void editorSelectText() {
    if (!E.is_selected)
        return;
    for (int i = 0; i < E.num_rows; i++) {
        memset(E.row[i].selected, 0, E.row[i].rsize);
    }
    int start_x, start_y, end_x, end_y;
    getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);
    start_x = editorRowCxToRx(&(E.row[start_y]), start_x);
    end_x = editorRowCxToRx(&(E.row[end_y]), end_x);

    if (start_y == end_y) {
        memset(&(E.row[E.cy].selected[start_x]), 1, end_x - start_x);
        return;
    }

    for (int i = start_y; i <= end_y; i++) {
        if (i == start_y) {
            memset(&(E.row[i].selected[start_x]), 1, E.row[i].rsize - start_x);
        } else if (i == end_y) {
            memset(E.row[i].selected, 1, end_x);
        } else {
            memset(E.row[i].selected, 1, E.row[i].rsize);
        }
    }
}

void editorDeleteSelectText() {
    int start_x, start_y, end_x, end_y;
    getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);
    E.cx = end_x;
    E.cy = end_y;
    if (end_y - start_y > 1) {
        for (int i = start_y + 1; i < end_y; i++) {
            editorFreeRow(&(E.row[i]));
        }
        int removed_rows = end_y - start_y - 1;
        memmove(&(E.row[start_y + 1]), &(E.row[end_y]),
                sizeof(EditorRow) * (E.num_rows - end_y));
        for (int i = start_y + 1; i < E.num_rows - removed_rows; i++) {
            E.row[i].idx -= removed_rows;
        }
        E.num_rows -= removed_rows;
        E.cy -= removed_rows;
        E.dirty++;

        E.num_rows_digits = 0;
        int num_rows = E.num_rows;
        while (num_rows) {
            num_rows /= 10;
            E.num_rows_digits++;
        }
    }
    while (E.cy != start_y || E.cx != start_x) {
        editorDelChar();
    }
}

void editorFreeClipboard(EditorClipboard* clipboard) {
    if (!clipboard || !clipboard->size)
        return;
    for (int i = 0; i < clipboard->size; i++) {
        free(clipboard->data[i]);
    }
    clipboard->size = 0;
    free(clipboard->data);
}

void editorCopySelectText() {
    if (!E.is_selected)
        return;

    int start_x, start_y, end_x, end_y;
    getSelectStartEnd(&start_x, &start_y, &end_x, &end_y);

    editorFreeClipboard(&E.clipboard);

    E.clipboard.size = end_y - start_y + 1;
    E.clipboard.data = malloc(sizeof(char*) * E.clipboard.size);

    // Only one line
    if (start_y == end_y) {
        E.clipboard.data[0] = malloc(end_x - start_x + 1);
        memcpy(E.clipboard.data[0], &E.row[start_y].data[start_x],
               end_x - start_x);
        E.clipboard.data[0][end_x - start_x] = '\0';
        return;
    }

    // First line
    size_t size = E.row[start_y].size - start_x;
    E.clipboard.data[0] = malloc(size + 1);
    memcpy(E.clipboard.data[0], &E.row[start_y].data[start_x], size);
    E.clipboard.data[0][size] = '\0';
    // Middle
    for (int i = start_y + 1; i < end_y; i++) {
        size = E.row[i].size;
        E.clipboard.data[i - start_y] = malloc(size + 1);
        memcpy(E.clipboard.data[i - start_y], E.row[i].data, size);
        E.clipboard.data[i - start_y][size] = '\0';
    }
    // Last line
    size = end_x;
    E.clipboard.data[end_y - start_y] = malloc(size + 1);
    memcpy(E.clipboard.data[end_y - start_y], E.row[end_y].data, size);
    E.clipboard.data[end_y - start_y][size] = '\0';
}

void editorPasteText() {
    if (!E.clipboard.size)
        return;
    int x = E.cx;
    int y = E.cy;
    if (E.clipboard.size == 1) {
        EditorRow* row = &E.row[y];
        char* paste = E.clipboard.data[0];
        size_t paste_len = strlen(paste);

        row->data = realloc(row->data, row->size + paste_len + 1);
        memmove(&(row->data[x + paste_len]), &(row->data[x]), row->size - x);
        memcpy(&(row->data[x]), paste, paste_len);
        row->size += paste_len;
        row->data[row->size] = '\0';
        editorUpdateRow(row);
        E.dirty++;
        E.cx += paste_len;
    } else {
        // First line
        int auto_indent = E.cfg->auto_indent;
        E.cfg->auto_indent = 0;
        editorInsertNewline();
        E.cfg->auto_indent = auto_indent;
        editorRowAppendString(&E.row[y], E.clipboard.data[0],
                              strlen(E.clipboard.data[0]));
        // Middle
        for (int i = 1; i < E.clipboard.size - 1; i++) {
            editorInsertRow(y + i, E.clipboard.data[i],
                            strlen(E.clipboard.data[i]));
        }
        // Last line
        EditorRow* row = &E.row[y + E.clipboard.size - 1];
        char* paste = E.clipboard.data[E.clipboard.size - 1];
        size_t paste_len = strlen(paste);

        row->data = realloc(row->data, row->size + paste_len + 1);
        memmove(&(row->data[paste_len]), row->data, row->size);
        memcpy(row->data, paste, paste_len);
        row->size += paste_len;
        row->data[row->size] = '\0';
        editorUpdateRow(row);

        E.cy = y + E.clipboard.size - 1;
        E.cx = paste_len;
    }
    E.sx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
}
