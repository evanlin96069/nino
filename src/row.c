#include "row.h"

#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "highlight.h"
#include "utils.h"

void editorUpdateRow(EditorFile* file, EditorRow* row) {
    row->rsize = editorRowCxToRx(row, row->size);
    editorUpdateSyntax(file, row);
}

void editorInsertRow(EditorFile* file, int at, const char* s, size_t len) {
    if (at < 0 || at > file->num_rows)
        return;

    file->row = realloc_s(file->row, sizeof(EditorRow) * (file->num_rows + 1));
    memmove(&file->row[at + 1], &file->row[at],
            sizeof(EditorRow) * (file->num_rows - at));

    file->row[at].size = len;
    file->row[at].data = malloc_s(len + 1);
    memcpy(file->row[at].data, s, len);
    file->row[at].data[len] = '\0';

    file->row[at].hl = NULL;
    file->row[at].hl_open_comment = 0;
    editorUpdateRow(file, &file->row[at]);

    file->num_rows++;
    file->num_rows_digits = getDigit(file->num_rows);
}

void editorFreeRow(EditorRow* row) {
    free(row->data);
    free(row->hl);
}

void editorDelRow(EditorFile* file, int at) {
    if (at < 0 || at >= file->num_rows)
        return;
    editorFreeRow(&file->row[at]);
    memmove(&file->row[at], &file->row[at + 1],
            sizeof(EditorRow) * (file->num_rows - at - 1));

    file->num_rows--;
    file->num_rows_digits = getDigit(file->num_rows);
}

void editorRowInsertChar(EditorFile* file, EditorRow* row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;
    row->data = realloc_s(row->data, row->size + 2);
    memmove(&row->data[at + 1], &row->data[at], row->size - at + 1);
    row->size++;
    row->data[at] = c;
    editorUpdateRow(file, row);
}

void editorRowDelChar(EditorFile* file, EditorRow* row, int at) {
    if (at < 0 || at >= row->size)
        return;
    memmove(&row->data[at], &row->data[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(file, row);
}

void editorRowAppendString(EditorFile* file, EditorRow* row, const char* s,
                           size_t len) {
    row->data = realloc_s(row->data, row->size + len + 1);
    memcpy(&row->data[row->size], s, len);
    row->size += len;
    row->data[row->size] = '\0';
    editorUpdateRow(file, row);
}

void editorInsertChar(int c) {
    if (gCurFile->cursor.y == gCurFile->num_rows) {
        editorInsertRow(gCurFile, gCurFile->num_rows, "", 0);
    }
    if (c == '\t' && CONVAR_GETINT(whitespace)) {
        int idx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                  gCurFile->cursor.x) +
                  1;
        editorInsertChar(' ');
        while (idx % CONVAR_GETINT(tabsize) != 0) {
            editorInsertChar(' ');
            idx++;
        }
    } else {
        editorRowInsertChar(gCurFile, &gCurFile->row[gCurFile->cursor.y],
                            gCurFile->cursor.x, c);
        gCurFile->cursor.x++;
    }
}

void editorInsertNewline(void) {
    int i = 0;

    if (gCurFile->cursor.x == 0) {
        editorInsertRow(gCurFile, gCurFile->cursor.y, "", 0);
    } else {
        editorInsertRow(gCurFile, gCurFile->cursor.y + 1, "", 0);
        EditorRow* curr_row = &gCurFile->row[gCurFile->cursor.y];
        EditorRow* new_row = &gCurFile->row[gCurFile->cursor.y + 1];
        if (CONVAR_GETINT(autoindent)) {
            while (i < gCurFile->cursor.x &&
                   (curr_row->data[i] == ' ' || curr_row->data[i] == '\t'))
                i++;
            if (i != 0)
                editorRowAppendString(gCurFile, new_row, curr_row->data, i);
            if (curr_row->data[gCurFile->cursor.x - 1] == ':' ||
                (curr_row->data[gCurFile->cursor.x - 1] == '{' &&
                 curr_row->data[gCurFile->cursor.x] != '}')) {
                if (CONVAR_GETINT(whitespace)) {
                    for (int j = 0; j < CONVAR_GETINT(tabsize); j++, i++)
                        editorRowAppendString(gCurFile, new_row, " ", 1);
                } else {
                    editorRowAppendString(gCurFile, new_row, "\t", 1);
                    i++;
                }
            }
        }
        editorRowAppendString(gCurFile, new_row,
                              &curr_row->data[gCurFile->cursor.x],
                              curr_row->size - gCurFile->cursor.x);
        curr_row->size = gCurFile->cursor.x;
        curr_row->data[curr_row->size] = '\0';
        editorUpdateRow(gCurFile, curr_row);
    }
    gCurFile->cursor.y++;
    gCurFile->cursor.x = i;
    gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], i);
}

void editorDelChar(void) {
    if (gCurFile->cursor.y == gCurFile->num_rows)
        return;
    if (gCurFile->cursor.x == 0 && gCurFile->cursor.y == 0)
        return;
    EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    if (gCurFile->cursor.x > 0) {
        editorRowDelChar(gCurFile, row, gCurFile->cursor.x - 1);
        gCurFile->cursor.x--;
    } else {
        gCurFile->cursor.x = gCurFile->row[gCurFile->cursor.y - 1].size;
        editorRowAppendString(gCurFile, &gCurFile->row[gCurFile->cursor.y - 1],
                              row->data, row->size);
        editorDelRow(gCurFile, gCurFile->cursor.y);
        gCurFile->cursor.y--;
    }
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}
