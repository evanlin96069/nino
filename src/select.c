#include "select.h"

#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "row.h"
#include "utils.h"

void getSelectStartEnd(EditorSelectRange* range) {
    if (gCurFile->cursor.select_y > gCurFile->cursor.y) {
        range->start_x = gCurFile->cursor.x;
        range->start_y = gCurFile->cursor.y;
        range->end_x = gCurFile->cursor.select_x;
        range->end_y = gCurFile->cursor.select_y;
    } else if (gCurFile->cursor.select_y < gCurFile->cursor.y) {
        range->start_x = gCurFile->cursor.select_x;
        range->start_y = gCurFile->cursor.select_y;
        range->end_x = gCurFile->cursor.x;
        range->end_y = gCurFile->cursor.y;
    } else {
        // same row
        range->start_y = range->end_y = gCurFile->cursor.y;
        range->start_x = gCurFile->cursor.select_x > gCurFile->cursor.x
                             ? gCurFile->cursor.x
                             : gCurFile->cursor.select_x;
        range->end_x = gCurFile->cursor.select_x > gCurFile->cursor.x
                           ? gCurFile->cursor.select_x
                           : gCurFile->cursor.x;
    }
}

bool isPosSelected(int row, int col, EditorSelectRange range) {
    if (range.start_y < row && row < range.end_y)
        return true;

    if (range.start_y == row && range.end_y == row)
        return range.start_x <= col && col < range.end_x;

    if (range.start_y == row)
        return range.start_x <= col;

    if (range.end_y == row)
        return col < range.end_x;

    return false;
}

void editorDeleteText(EditorSelectRange range) {
    if (range.start_x == range.end_x && range.start_y == range.end_y)
        return;

    gCurFile->cursor.x = range.end_x;
    gCurFile->cursor.y = range.end_y;

    if (range.end_y - range.start_y > 1) {
        for (int i = range.start_y + 1; i < range.end_y; i++) {
            editorFreeRow(&(gCurFile->row[i]));
        }
        int removed_rows = range.end_y - range.start_y - 1;
        memmove(&(gCurFile->row[range.start_y + 1]),
                &(gCurFile->row[range.end_y]),
                sizeof(EditorRow) * (gCurFile->num_rows - range.end_y));

        gCurFile->num_rows -= removed_rows;
        gCurFile->cursor.y -= removed_rows;

        gCurFile->num_rows_digits = 0;
        int num_rows = gCurFile->num_rows;
        while (num_rows) {
            num_rows /= 10;
            gCurFile->num_rows_digits++;
        }
    }
    while (gCurFile->cursor.y != range.start_y ||
           gCurFile->cursor.x != range.start_x) {
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
        memcpy(clipboard->data[0],
               &gCurFile->row[range.start_y].data[range.start_x],
               range.end_x - range.start_x);
        clipboard->data[0][range.end_x - range.start_x] = '\0';
        return;
    }

    // First line
    size_t size = gCurFile->row[range.start_y].size - range.start_x;
    clipboard->data[0] = malloc_s(size + 1);
    memcpy(clipboard->data[0],
           &gCurFile->row[range.start_y].data[range.start_x], size);
    clipboard->data[0][size] = '\0';

    // Middle
    for (int i = range.start_y + 1; i < range.end_y; i++) {
        size = gCurFile->row[i].size;
        clipboard->data[i - range.start_y] = malloc_s(size + 1);
        memcpy(clipboard->data[i - range.start_y], gCurFile->row[i].data, size);
        clipboard->data[i - range.start_y][size] = '\0';
    }
    // Last line
    size = range.end_x;
    clipboard->data[range.end_y - range.start_y] = malloc_s(size + 1);
    memcpy(clipboard->data[range.end_y - range.start_y],
           gCurFile->row[range.end_y].data, size);
    clipboard->data[range.end_y - range.start_y][size] = '\0';
}

void editorPasteText(const EditorClipboard* clipboard, int x, int y) {
    if (!clipboard->size)
        return;

    gCurFile->cursor.x = x;
    gCurFile->cursor.y = y;

    if (clipboard->size == 1) {
        EditorRow* row = &gCurFile->row[y];
        char* paste = clipboard->data[0];
        size_t paste_len = strlen(paste);

        row->data = realloc_s(row->data, row->size + paste_len + 1);
        memmove(&row->data[x + paste_len], &row->data[x], row->size - x);
        memcpy(&row->data[x], paste, paste_len);
        row->size += paste_len;
        row->data[row->size] = '\0';
        editorUpdateRow(gCurFile, row);
        gCurFile->cursor.x += paste_len;
    } else {
        // First line
        int auto_indent = CONVAR_GETINT(autoindent);
        CONVAR_GETINT(autoindent) = 0;
        editorInsertNewline();
        CONVAR_GETINT(autoindent) = auto_indent;
        editorRowAppendString(&gCurFile->row[y], clipboard->data[0],
                              strlen(clipboard->data[0]));
        // Middle
        for (size_t i = 1; i < clipboard->size - 1; i++) {
            editorInsertRow(y + i, clipboard->data[i],
                            strlen(clipboard->data[i]));
        }
        // Last line
        EditorRow* row = &gCurFile->row[y + clipboard->size - 1];
        char* paste = clipboard->data[clipboard->size - 1];
        size_t paste_len = strlen(paste);

        row->data = realloc_s(row->data, row->size + paste_len + 1);
        memmove(&row->data[paste_len], row->data, row->size);
        memcpy(row->data, paste, paste_len);
        row->size += paste_len;
        row->data[row->size] = '\0';
        editorUpdateRow(gCurFile, row);

        gCurFile->cursor.y = y + clipboard->size - 1;
        gCurFile->cursor.x = paste_len;
    }
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}

void editorFreeClipboardContent(EditorClipboard* clipboard) {
    if (!clipboard || !clipboard->size)
        return;
    for (size_t i = 0; i < clipboard->size; i++) {
        free(clipboard->data[i]);
    }
    clipboard->size = 0;
    free(clipboard->data);
}
