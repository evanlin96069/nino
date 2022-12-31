#include "select.h"

#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "row.h"
#include "utils.h"

void getSelectStartEnd(EditorSelectRange* range) {
    if (E.cursor.select_y > E.cursor.y) {
        range->start_x = E.cursor.x;
        range->start_y = E.cursor.y;
        range->end_x = E.cursor.select_x;
        range->end_y = E.cursor.select_y;
    } else if (E.cursor.select_y < E.cursor.y) {
        range->start_x = E.cursor.select_x;
        range->start_y = E.cursor.select_y;
        range->end_x = E.cursor.x;
        range->end_y = E.cursor.y;
    } else {
        // same row
        range->start_y = range->end_y = E.cursor.y;
        range->start_x =
            E.cursor.select_x > E.cursor.x ? E.cursor.x : E.cursor.select_x;
        range->end_x =
            E.cursor.select_x > E.cursor.x ? E.cursor.select_x : E.cursor.x;
    }
}

void editorSelectText() {
    if (!E.cursor.is_selected)
        return;
    for (int i = 0; i < E.num_rows; i++) {
        memset(E.row[i].selected, 0, E.row[i].rsize);
    }
    EditorSelectRange range;
    getSelectStartEnd(&range);
    range.start_x = editorRowCxToRx(&(E.row[range.start_y]), range.start_x);
    range.end_x = editorRowCxToRx(&(E.row[range.end_y]), range.end_x);

    if (range.start_y == range.end_y) {
        memset(&(E.row[E.cursor.y].selected[range.start_x]), 1,
               range.end_x - range.start_x);
        return;
    }

    for (int i = range.start_y; i <= range.end_y; i++) {
        if (i == range.start_y) {
            memset(&(E.row[i].selected[range.start_x]), 1,
                   E.row[i].rsize - range.start_x);
        } else if (i == range.end_y) {
            memset(E.row[i].selected, 1, range.end_x);
        } else {
            memset(E.row[i].selected, 1, E.row[i].rsize);
        }
    }
}

void editorDeleteText(EditorSelectRange range) {
    if (range.start_x == range.end_x && range.start_y == range.end_y)
        return;

    E.cursor.x = range.end_x;
    E.cursor.y = range.end_y;

    if (range.end_y - range.start_y > 1) {
        for (int i = range.start_y + 1; i < range.end_y; i++) {
            editorFreeRow(&(E.row[i]));
        }
        int removed_rows = range.end_y - range.start_y - 1;
        memmove(&(E.row[range.start_y + 1]), &(E.row[range.end_y]),
                sizeof(EditorRow) * (E.num_rows - range.end_y));
        for (int i = range.start_y + 1; i < E.num_rows - removed_rows; i++) {
            E.row[i].idx -= removed_rows;
        }
        E.num_rows -= removed_rows;
        E.cursor.y -= removed_rows;

        E.num_rows_digits = 0;
        int num_rows = E.num_rows;
        while (num_rows) {
            num_rows /= 10;
            E.num_rows_digits++;
        }
    }
    while (E.cursor.y != range.start_y || E.cursor.x != range.start_x) {
        editorDelChar();
    }
}

void editorCopyText(EditorClipboard* clipboard, EditorSelectRange range) {
    if (range.start_x == range.end_x && range.start_y == range.end_y) {
        clipboard->size = 0;
        clipboard->data = NULL;
        return;
    }

    clipboard->size = range.end_y - range.start_y + 1;
    clipboard->data = malloc_s(sizeof(char*) * clipboard->size);
    // Only one line
    if (range.start_y == range.end_y) {
        clipboard->data[0] = malloc_s(range.end_x - range.start_x + 1);
        memcpy(clipboard->data[0], &E.row[range.start_y].data[range.start_x],
               range.end_x - range.start_x);
        clipboard->data[0][range.end_x - range.start_x] = '\0';
        return;
    }

    // First line
    size_t size = E.row[range.start_y].size - range.start_x;
    clipboard->data[0] = malloc_s(size + 1);
    memcpy(clipboard->data[0], &E.row[range.start_y].data[range.start_x], size);
    clipboard->data[0][size] = '\0';

    // Middle
    for (int i = range.start_y + 1; i < range.end_y; i++) {
        size = E.row[i].size;
        clipboard->data[i - range.start_y] = malloc_s(size + 1);
        memcpy(clipboard->data[i - range.start_y], E.row[i].data, size);
        clipboard->data[i - range.start_y][size] = '\0';
    }
    // Last line
    size = range.end_x;
    clipboard->data[range.end_y - range.start_y] = malloc_s(size + 1);
    memcpy(clipboard->data[range.end_y - range.start_y],
           E.row[range.end_y].data, size);
    clipboard->data[range.end_y - range.start_y][size] = '\0';
}

void editorPasteText(const EditorClipboard* clipboard, int x, int y) {
    if (!clipboard->size)
        return;

    E.cursor.x = x;
    E.cursor.y = y;

    if (clipboard->size == 1) {
        EditorRow* row = &E.row[y];
        char* paste = clipboard->data[0];
        size_t paste_len = strlen(paste);

        row->data = realloc_s(row->data, row->size + paste_len + 1);
        memmove(&(row->data[x + paste_len]), &(row->data[x]), row->size - x);
        memcpy(&(row->data[x]), paste, paste_len);
        row->size += paste_len;
        row->data[row->size] = '\0';
        editorUpdateRow(row);
        E.cursor.x += paste_len;
    } else {
        // First line
        int auto_indent = E.cfg->auto_indent;
        E.cfg->auto_indent = 0;
        editorInsertNewline();
        E.cfg->auto_indent = auto_indent;
        editorRowAppendString(&E.row[y], clipboard->data[0],
                              strlen(clipboard->data[0]));
        // Middle
        for (int i = 1; i < clipboard->size - 1; i++) {
            editorInsertRow(y + i, clipboard->data[i],
                            strlen(clipboard->data[i]));
        }
        // Last line
        EditorRow* row = &E.row[y + clipboard->size - 1];
        char* paste = clipboard->data[clipboard->size - 1];
        size_t paste_len = strlen(paste);

        row->data = realloc_s(row->data, row->size + paste_len + 1);
        memmove(&(row->data[paste_len]), row->data, row->size);
        memcpy(row->data, paste, paste_len);
        row->size += paste_len;
        row->data[row->size] = '\0';
        editorUpdateRow(row);

        E.cursor.y = y + clipboard->size - 1;
        E.cursor.x = paste_len;
    }
    E.sx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
}

void editorFreeClipboardContent(EditorClipboard* clipboard) {
    if (!clipboard || !clipboard->size)
        return;
    for (int i = 0; i < clipboard->size; i++) {
        free(clipboard->data[i]);
    }
    clipboard->size = 0;
    free(clipboard->data);
}
