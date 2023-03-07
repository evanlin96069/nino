#include "row.h"

#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "highlight.h"

static inline int getDigit(int n) {
    if (n < 10)
        return 1;
    if (n < 100)
        return 2;
    if (n < 1000)
        return 3;
    if (n < 10000000) {
        if (n < 1000000) {
            if (n < 10000)
                return 4;
            return 5 + (n >= 100000);
        }
        return 7;
    }
    if (n < 1000000000)
        return 8 + (n >= 100000000);
    return 10;
}

static void editorUpdateNumRowsDigits() {
    int old_digit = gCurFile->num_rows_digits;
    int digits = getDigit(gCurFile->num_rows);
    if (old_digit != digits) {
        gCurFile->num_rows_digits = digits;
    }
}

void editorUpdateRow(EditorRow* row) {
    row->rsize = editorRowCxToRx(row, row->size);
    editorUpdateSyntax(row);
}

void editorInsertRow(int at, const char* s, size_t len) {
    if (at < 0 || at > gCurFile->num_rows)
        return;

    gCurFile->row =
        realloc_s(gCurFile->row, sizeof(EditorRow) * (gCurFile->num_rows + 1));
    memmove(&(gCurFile->row[at + 1]), &(gCurFile->row[at]),
            sizeof(EditorRow) * (gCurFile->num_rows - at));
    for (int i = at + 1; i <= gCurFile->num_rows; i++) {
        gCurFile->row[i].idx++;
    }

    gCurFile->row[at].idx = at;

    gCurFile->row[at].size = len;
    gCurFile->row[at].data = malloc_s(len + 1);
    memcpy(gCurFile->row[at].data, s, len);
    gCurFile->row[at].data[len] = '\0';

    gCurFile->row[at].hl = NULL;
    gCurFile->row[at].hl_open_comment = 0;
    editorUpdateRow(&(gCurFile->row[at]));

    gCurFile->num_rows++;

    editorUpdateNumRowsDigits();
}

void editorFreeRow(EditorRow* row) {
    free(row->data);
    free(row->hl);
}

void editorDelRow(int at) {
    if (at < 0 || at >= gCurFile->num_rows)
        return;
    editorFreeRow(&(gCurFile->row[at]));
    memmove(&(gCurFile->row[at]), &(gCurFile->row[at + 1]),
            sizeof(EditorRow) * (gCurFile->num_rows - at - 1));
    for (int i = at; i < gCurFile->num_rows - 1; i++) {
        gCurFile->row[i].idx--;
    }
    gCurFile->num_rows--;

    editorUpdateNumRowsDigits();
}

void editorRowInsertChar(EditorRow* row, int at, int c) {
    if (at < 0 || at > row->size)
        at = row->size;
    row->data = realloc_s(row->data, row->size + 2);
    memmove(&(row->data[at + 1]), &(row->data[at]), row->size - at + 1);
    row->size++;
    row->data[at] = c;
    editorUpdateRow(row);
}

void editorRowDelChar(EditorRow* row, int at) {
    if (at < 0 || at >= row->size)
        return;
    memmove(&(row->data[at]), &(row->data[at + 1]), row->size - at);
    row->size--;
    editorUpdateRow(row);
}

void editorRowAppendString(EditorRow* row, const char* s, size_t len) {
    row->data = realloc_s(row->data, row->size + len + 1);
    memcpy(&(row->data[row->size]), s, len);
    row->size += len;
    row->data[row->size] = '\0';
    editorUpdateRow(row);
}
