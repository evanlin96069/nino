#include <stdlib.h>
#include <string.h>
#include "select.h"
#include "editor.h"
#include "row.h"
#include "utils.h"

void getSelectStartEnd(int* start_x, int* start_y, int* end_x, int* end_y) {
    if (E.select_y > E.cy) {
        *start_x = E.cx;
        *start_y = E.cy;
        *end_x = E.select_x;
        *end_y = E.select_y;
    }
    else if (E.select_y < E.cy) {
        *start_x = E.select_x;
        *start_y = E.select_y;
        *end_x = E.cx;
        *end_y = E.cy;
    }
    else {
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
        }
        else if (i == end_y) {
            memset(E.row[i].selected, 1, end_x);
        }
        else {
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
        memmove(&(E.row[start_y + 1]), &(E.row[end_y]), sizeof(EditorRow) * (E.num_rows - end_y));
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
