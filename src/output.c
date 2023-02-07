#include "output.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "defines.h"
#include "editor.h"
#include "highlight.h"
#include "select.h"
#include "status.h"

void editorScroll() {
    E.rx = 0;
    if (E.cursor.y < E.num_rows) {
        E.rx = editorRowCxToRx(&(E.row[E.cursor.y]), E.cursor.x);
    }

    if (E.cursor.y < E.row_offset) {
        E.row_offset = E.cursor.y;
    }
    if (E.cursor.y >= E.row_offset + E.rows) {
        E.row_offset = E.cursor.y - E.rows + 1;
    }
    if (E.rx < E.col_offset) {
        E.col_offset = E.rx;
    }
    if (E.rx >= E.col_offset + E.cols) {
        E.col_offset = E.rx - E.cols + 1;
    }
}

void editorDrawRows(abuf* ab) {
    EditorSelectRange range = {0};
    if (E.cursor.is_selected)
        getSelectStartEnd(&range);

    for (int i = E.row_offset; i < E.row_offset + E.rows; i++) {
        if (i < E.num_rows) {
            char line_number[16];
            char buf[32];
            if (i == E.cursor.y) {
                colorToANSI(E.color_cfg.line_number[1], buf, 0);
                abufAppend(ab, buf);
                colorToANSI(E.color_cfg.line_number[0], buf, 1);
                abufAppend(ab, buf);
            } else {
                colorToANSI(E.color_cfg.line_number[0], buf, 0);
                abufAppend(ab, buf);
                colorToANSI(E.color_cfg.line_number[1], buf, 1);
                abufAppend(ab, buf);
            }
            snprintf(line_number, sizeof(line_number), "%*d ",
                     E.num_rows_digits, i + 1);
            abufAppend(ab, line_number);

            abufAppend(ab, ANSI_CLEAR);
            colorToANSI(E.color_cfg.bg, buf, 1);
            abufAppend(ab, buf);

            int len = E.row[i].rsize - E.col_offset;
            if (len < 0)
                len = 0;
            if (len > E.cols)
                len = E.cols;
            char* c = &(E.row[i].render[E.col_offset]);
            unsigned char* hl = &(E.row[i].hl[E.col_offset]);
            unsigned char current_color = HL_NORMAL;
            bool in_select = false;
            bool has_bg = false;
            colorToANSI(E.color_cfg.highlight[current_color], buf, 0);
            abufAppend(ab, buf);
            for (int j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, ANSI_INVERT);
                    abufAppendN(ab, &sym, 1);

                    abufAppend(ab, ANSI_CLEAR);
                    colorToANSI(E.color_cfg.bg, buf, 1);
                    abufAppend(ab, buf);
                    colorToANSI(E.color_cfg.highlight[current_color], buf, 0);
                    abufAppend(ab, buf);
                } else {
                    unsigned char color = hl[j];
                    if (E.cursor.is_selected &&
                        isPosSelected(
                            i, editorRowRxToCx(&E.row[i], j + E.col_offset),
                            range)) {
                        if (!in_select) {
                            in_select = true;
                            colorToANSI(E.color_cfg.highlight[HL_SELECT], buf,
                                        1);
                            abufAppend(ab, buf);
                        }
                    } else {
                        in_select = false;
                        // restore bg
                        if (color == HL_MATCH || color == HL_SPACE) {
                            colorToANSI(E.color_cfg.highlight[color], buf, 1);
                            abufAppend(ab, buf);
                        } else {
                            colorToANSI(E.color_cfg.bg, buf, 1);
                            abufAppend(ab, buf);
                        }
                    }

                    if (color != current_color) {
                        current_color = color;
                        if (color == HL_MATCH || color == HL_SPACE) {
                            has_bg = true;
                            colorToANSI(E.color_cfg.highlight[HL_NORMAL], buf,
                                        0);
                            abufAppend(ab, buf);
                            if (!in_select) {
                                colorToANSI(E.color_cfg.highlight[color], buf,
                                            1);
                                abufAppend(ab, buf);
                            }
                        } else {
                            if (has_bg) {
                                has_bg = false;
                                if (!in_select) {
                                    colorToANSI(E.color_cfg.bg, buf, 1);
                                    abufAppend(ab, buf);
                                }
                            }
                            colorToANSI(E.color_cfg.highlight[color], buf, 0);
                            abufAppend(ab, buf);
                        }
                    }
                    abufAppendN(ab, &c[j], 1);
                }
            }
            // Add newline character when selected
            if (E.cursor.is_selected && range.end_y > i && i >= range.start_y &&
                E.row[i].rsize - E.col_offset < E.cols) {
                colorToANSI(E.color_cfg.highlight[HL_SELECT], buf, 1);
                abufAppend(ab, buf);
                abufAppend(ab, " ");
            }
            abufAppend(ab, ANSI_CLEAR);
            colorToANSI(E.color_cfg.bg, buf, 1);
            abufAppend(ab, buf);
        }
        abufAppend(ab, "\x1b[K");
        abufAppend(ab, "\r\n");
    }
}

void editorRefreshScreen() {
    abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l");
    abufAppend(&ab, "\x1b[H");

    editorDrawTopStatusBar(&ab);
    editorDrawRows(&ab);
    editorDrawStatusMsgBar(&ab);
    editorDrawStatusBar(&ab);

    char buf[32];
    bool should_show_cursor = true;
    if (E.state == EDIT_MODE) {
        int row = (E.cursor.y - E.row_offset) + 2;
        int col = (E.rx - E.col_offset) + 1 + E.num_rows_digits + 1;
        if (row <= 1 || row > E.screen_rows - 2)
            should_show_cursor = false;
        else
            snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    } else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.rows + 2, E.px + 1);
    }
    abufAppend(&ab, buf);

    if (should_show_cursor)
        abufAppend(&ab, "\x1b[?25h");

    UNUSED(write(STDOUT_FILENO, ab.buf, ab.len));
    abufFree(&ab);
}
