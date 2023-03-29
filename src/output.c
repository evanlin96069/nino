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
#include "unicode.h"

void editorDrawRows(abuf* ab) {
    EditorSelectRange range = {0};
    if (gCurFile->cursor.is_selected)
        getSelectStartEnd(&range);

    for (int i = gCurFile->row_offset, s_row = 2;
         i < gCurFile->row_offset + gEditor.display_rows; i++, s_row++) {
        char buf[32];
        bool is_row_full = false;
        // Move cursor to the beginning of a row, in case the charater width
        // is calculated incorrectly.
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", s_row, 0);
        abufAppend(ab, buf);

        if (i < gCurFile->num_rows) {
            char line_number[16];
            if (i == gCurFile->cursor.y) {
                colorToANSI(gEditor.color_cfg.line_number[1], buf, 0);
                abufAppend(ab, buf);
                colorToANSI(gEditor.color_cfg.line_number[0], buf, 1);
                abufAppend(ab, buf);
            } else {
                colorToANSI(gEditor.color_cfg.line_number[0], buf, 0);
                abufAppend(ab, buf);
                colorToANSI(gEditor.color_cfg.line_number[1], buf, 1);
                abufAppend(ab, buf);
            }

            snprintf(line_number, sizeof(line_number), "%*d ",
                     gCurFile->num_rows_digits, i + 1);
            abufAppend(ab, line_number);

            abufAppend(ab, ANSI_CLEAR);
            colorToANSI(gEditor.color_cfg.bg, buf, 1);
            abufAppend(ab, buf);

            int cols = gEditor.screen_cols - (gCurFile->num_rows_digits + 1);
            int col_offset =
                editorRowRxToCx(&gCurFile->row[i], gCurFile->col_offset);
            int len = gCurFile->row[i].size - col_offset;
            len = (len < 0) ? 0 : len;

            int rlen = gCurFile->row[i].rsize - gCurFile->col_offset;
            is_row_full = (rlen > cols);
            rlen = is_row_full ? cols : rlen;
            rlen += gCurFile->col_offset;

            char* c = &gCurFile->row[i].data[col_offset];
            unsigned char* hl = &(gCurFile->row[i].hl[col_offset]);
            unsigned char current_color = HL_NORMAL;

            bool in_select = false;
            bool has_bg = false;

            colorToANSI(gEditor.color_cfg.highlight[current_color], buf, 0);
            abufAppend(ab, buf);

            int j = 0;
            int rx = gCurFile->col_offset;
            while (rx < rlen) {
                if (iscntrl(c[j]) && c[j] != '\t') {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, ANSI_INVERT);
                    abufAppendN(ab, &sym, 1);

                    abufAppend(ab, ANSI_CLEAR);
                    colorToANSI(gEditor.color_cfg.bg, buf, 1);
                    abufAppend(ab, buf);
                    colorToANSI(gEditor.color_cfg.highlight[current_color], buf,
                                0);
                    abufAppend(ab, buf);

                    rx++;
                    j++;
                } else {
                    unsigned char color = hl[j];
                    if (gCurFile->cursor.is_selected &&
                        isPosSelected(i, j + col_offset, range)) {
                        if (!in_select) {
                            in_select = true;
                            colorToANSI(gEditor.color_cfg.highlight[HL_SELECT],
                                        buf, 1);
                            abufAppend(ab, buf);
                        }
                    } else {
                        // restore bg
                        if (color == HL_MATCH || color == HL_SPACE) {
                            colorToANSI(gEditor.color_cfg.highlight[color], buf,
                                        1);
                            abufAppend(ab, buf);
                        } else if (in_select) {
                            in_select = false;
                            colorToANSI(gEditor.color_cfg.bg, buf, 1);
                            abufAppend(ab, buf);
                        }
                    }

                    if (color != current_color) {
                        current_color = color;
                        if (color == HL_MATCH || color == HL_SPACE) {
                            has_bg = true;
                            colorToANSI(gEditor.color_cfg.highlight[HL_NORMAL],
                                        buf, 0);
                            abufAppend(ab, buf);
                            if (!in_select) {
                                colorToANSI(gEditor.color_cfg.highlight[color],
                                            buf, 1);
                                abufAppend(ab, buf);
                            }
                        } else {
                            if (has_bg) {
                                has_bg = false;
                                if (!in_select) {
                                    colorToANSI(gEditor.color_cfg.bg, buf, 1);
                                    abufAppend(ab, buf);
                                }
                            }
                            colorToANSI(gEditor.color_cfg.highlight[color], buf,
                                        0);
                            abufAppend(ab, buf);
                        }
                    }
                    if (c[j] == '\t') {
                        // TODO: Add show tab feature
                        abufAppend(ab, " ");
                        rx++;
                        while (rx % CONVAR_GETINT(tabsize) != 0 && rx < rlen) {
                            abufAppend(ab, " ");
                            rx++;
                        }
                        j++;
                    } else {
                        size_t byte_size;
                        uint32_t unicode =
                            decodeUTF8(&c[j], len - j, &byte_size);
                        int width = unicodeWidth(unicode);
                        if (width >= 0) {
                            rx += width;
                            // Make sure double won't exceed the screen
                            if (rx <= rlen)
                                abufAppendN(ab, &c[j], byte_size);
                        }
                        j += byte_size;
                    }
                }
            }
            // Add newline character when selected
            if (gCurFile->cursor.is_selected && range.end_y > i &&
                i >= range.start_y &&
                gCurFile->row[i].rsize - gCurFile->col_offset < cols) {
                colorToANSI(gEditor.color_cfg.highlight[HL_SELECT], buf, 1);
                abufAppend(ab, buf);
                abufAppend(ab, " ");
            }
            abufAppend(ab, ANSI_CLEAR);
            colorToANSI(gEditor.color_cfg.bg, buf, 1);
            abufAppend(ab, buf);
        }

        if (!is_row_full) {
            abufAppend(ab, "\x1b[K");
        }
    }
}

void editorRefreshScreen() {
    abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l");
    abufAppend(&ab, "\x1b[H");

    editorDrawTopStatusBar(&ab);
    editorDrawRows(&ab);
    editorDrawPrompt(&ab);
    editorDrawStatusBar(&ab);

    char buf[32];
    bool should_show_cursor = true;
    if (gEditor.state == EDIT_MODE) {
        int row = (gCurFile->cursor.y - gCurFile->row_offset) + 2;
        int col = (editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                   gCurFile->cursor.x) -
                   gCurFile->col_offset) +
                  1 + gCurFile->num_rows_digits + 1;
        if (row <= 1 || row > gEditor.screen_rows - 2 || col <= 1 ||
            col > gEditor.screen_cols)
            should_show_cursor = false;
        else
            snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row, col);
    } else {
        snprintf(buf, sizeof(buf), "\x1b[%d;%dH", gEditor.display_rows + 2,
                 gEditor.px + 1);
    }
    abufAppend(&ab, buf);

    if (should_show_cursor)
        abufAppend(&ab, "\x1b[?25h");
    else
        abufAppend(&ab, "\x1b[?25l");

    UNUSED(write(STDOUT_FILENO, ab.buf, ab.len));
    abufFree(&ab);
}
