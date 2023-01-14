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
    int old_digit = E.num_rows_digits;
    int digits = getDigit(E.num_rows);
    if (old_digit != digits) {
        E.num_rows_digits = digits;
        E.cols = E.screen_cols - (E.num_rows_digits + 1);
    }
}

void editorUpdateRow(EditorRow* row) {
    int tabs = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->data[i] == '\t')
            tabs++;
    }

    free(row->render);
    row->render = malloc_s(row->size + tabs * (CONVAR_GETINT(tabsize) - 1) + 1);

    int idx = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->data[i] == '\t') {
            row->render[idx++] = ' ';
            while (idx % CONVAR_GETINT(tabsize) != 0)
                row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->data[i];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

void editorInsertRow(int at, const char* s, size_t len) {
    if (at < 0 || at > E.num_rows)
        return;

    E.row = realloc_s(E.row, sizeof(EditorRow) * (E.num_rows + 1));
    memmove(&(E.row[at + 1]), &(E.row[at]),
            sizeof(EditorRow) * (E.num_rows - at));
    for (int i = at + 1; i <= E.num_rows; i++) {
        E.row[i].idx++;
    }

    E.row[at].idx = at;

    E.row[at].size = len;
    E.row[at].data = malloc_s(len + 1);
    memcpy(E.row[at].data, s, len);
    E.row[at].data[len] = '\0';

    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&(E.row[at]));

    E.num_rows++;

    editorUpdateNumRowsDigits();
}

void editorFreeRow(EditorRow* row) {
    free(row->render);
    free(row->data);
    free(row->hl);
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.num_rows)
        return;
    editorFreeRow(&(E.row[at]));
    memmove(&(E.row[at]), &(E.row[at + 1]),
            sizeof(EditorRow) * (E.num_rows - at - 1));
    for (int i = at; i < E.num_rows - 1; i++) {
        E.row[i].idx--;
    }
    E.num_rows--;

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
