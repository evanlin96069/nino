#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "output.h"
#include "editor.h"
#include "select.h"
#include "highlight.h"
#include "defines.h"
#include "status.h"

void editorScroll() {
    E.rx = 0;
    if (E.cy < E.num_rows) {
        E.rx = editorRowCxToRx(&(E.row[E.cy]), E.cx);
    }

    if (E.cy < E.row_offset) {
        E.row_offset = E.cy;
    }
    if (E.cy >= E.row_offset + E.rows) {
        E.row_offset = E.cy - E.rows + 1;
    }
    if (E.rx < E.col_offset) {
        E.col_offset = E.rx;
    }
    if (E.rx >= E.col_offset + E.cols) {
        E.col_offset = E.rx - E.cols + 1;
    }
}

void editorDrawRows(abuf* ab) {
    editorSelectText();
    for (int i = 0; i < E.rows; i++) {
        int current_row = i + E.row_offset;
        if (current_row >= E.num_rows) {
            abufAppend(ab, "~", 1);
        }
        else {
            char line_number[16];
            if (current_row == E.cy) {
                abufAppend(ab, "\x1b[30;100m", 9);
            }
            else {
                abufAppend(ab, "\x1b[90m", 5);
            }
            int nlen = snprintf(line_number, sizeof(line_number), "%*d ", E.num_rows_digits, current_row + 1);
            abufAppend(ab, line_number, nlen);
            abufAppend(ab, "\x1b[m", 3);
            int len = E.row[current_row].rsize - E.col_offset;
            if (len < 0)
                len = 0;
            if (len > E.cols)
                len = E.cols;
            char* c = &(E.row[current_row].render[E.col_offset]);
            unsigned char* hl = &(E.row[current_row].hl[E.col_offset]);
            unsigned char* selected = &(E.row[current_row].selected[E.col_offset]);
            int current_color = -1;
            for (int j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, "\x1b[7m", 4);
                    abufAppend(ab, &sym, 1);
                    abufAppend(ab, "\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abufAppend(ab, buf, clen);
                    }
                }
                else if (E.is_selected && selected[j]) {
                    current_color = -2;
                    abufAppend(ab, "\x1b[30;47m", 8);
                    abufAppend(ab, &c[j], 1);
                    abufAppend(ab, "\x1b[m", 3);

                }
                else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abufAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abufAppend(ab, &c[j], 1);
                }
                else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abufAppend(ab, buf, clen);
                    }
                    abufAppend(ab, &c[j], 1);
                }
            }
            abufAppend(ab, "\x1b[39m", 5);
        }

        abufAppend(ab, "\x1b[K", 3);
        abufAppend(ab, "\r\n", 2);
    }
}

void editorRefreshScreen() {
    editorScroll();

    abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l", 6);
    abufAppend(&ab, "\x1b[H", 3);

    editorDrawTopStatusBar(&ab);
    editorDrawRows(&ab);
    editorDrawStatusMsgBar(&ab);
    editorDrawStatusBar(&ab);

    char buf[32];
    if (E.state == EDIT_MODE) {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
            (E.cy - E.row_offset) + 2,
            (E.rx - E.col_offset) + 1 + E.num_rows_digits + 1);
    }
    else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
            E.rows + 2,
            E.px + 1);
    }

    abufAppend(&ab, buf, strlen(buf));

    abufAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.buf, ab.len);
    abufFree(&ab);
}
