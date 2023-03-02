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
        E.rx = editorRowCxToRx(&E.row[E.cursor.y], E.cursor.x);
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

            int col_offset = editorRowRxToCx(&E.row[i], E.col_offset);
            int rlen = E.row[i].rsize - E.col_offset;
            rlen = rlen > E.cols ? E.cols : rlen;
            rlen += E.col_offset;

            char* c = &E.row[i].data[col_offset];
            unsigned char* hl = &(E.row[i].hl[col_offset]);
            unsigned char current_color = HL_NORMAL;

            bool in_select = false;
            bool has_bg = false;

            colorToANSI(E.color_cfg.highlight[current_color], buf, 0);
            abufAppend(ab, buf);

            // If rlen is calculated correctly, j shouldn't go out of bounds
            for (int j = 0, rx = E.col_offset; rx < rlen; j++, rx++) {
                if (iscntrl(c[j]) && c[j] != '\t') {
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
                        isPosSelected(i, j + col_offset, range)) {
                        if (!in_select) {
                            in_select = true;
                            colorToANSI(E.color_cfg.highlight[HL_SELECT], buf,
                                        1);
                            abufAppend(ab, buf);
                        }
                    } else {
                        // restore bg
                        if (color == HL_MATCH || color == HL_SPACE) {
                            colorToANSI(E.color_cfg.highlight[color], buf, 1);
                            abufAppend(ab, buf);
                        } else if (in_select) {
                            in_select = false;
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
                    if (c[j] == '\t') {
                        // TODO: Add show tab feature
                        abufAppend(ab, " ");
                        while ((rx + 1) % CONVAR_GETINT(tabsize) != 0 &&
                               (rx + 1) < rlen) {
                            abufAppend(ab, " ");
                            rx++;
                        }
                    } else {
                        abufAppendN(ab, &c[j], 1);
                    }
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
        if (row <= 1 || row > E.screen_rows - 2 || col <= 1 ||
            col > E.screen_cols)
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
