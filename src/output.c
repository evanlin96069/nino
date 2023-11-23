#include "output.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"
#include "editor.h"
#include "highlight.h"
#include "os.h"
#include "select.h"
#include "status.h"
#include "unicode.h"

void editorDrawRows(abuf* ab) {
    EditorSelectRange range = {0};
    if (gCurFile->cursor.is_selected)
        getSelectStartEnd(&range);

    for (int i = gCurFile->row_offset, s_row = 2;
         i < gCurFile->row_offset + gEditor.display_rows; i++, s_row++) {
        bool is_row_full = false;
        // Move cursor to the beginning of a row
        gotoXY(ab, s_row, 1 + gEditor.explorer.width);

        gEditor.color_cfg.highlightBg[HL_BG_NORMAL] = gEditor.color_cfg.bg;
        if (i < gCurFile->num_rows) {
            char line_number[16];
            if (i == gCurFile->cursor.y) {
                if (!gCurFile->cursor.is_selected) {
                    gEditor.color_cfg.highlightBg[HL_BG_NORMAL] =
                        gEditor.color_cfg.cursor_line;
                }
                setColor(ab, gEditor.color_cfg.line_number[1], 0);
                setColor(ab, gEditor.color_cfg.line_number[0], 1);
            } else {
                setColor(ab, gEditor.color_cfg.line_number[0], 0);
                setColor(ab, gEditor.color_cfg.line_number[1], 1);
            }

            snprintf(line_number, sizeof(line_number), " %*d ",
                     gCurFile->lineno_width - 2, i + 1);
            abufAppend(ab, line_number);

            abufAppend(ab, ANSI_CLEAR);
            setColor(ab, gEditor.color_cfg.bg, 1);

            int cols = gEditor.screen_cols - gEditor.explorer.width -
                       gCurFile->lineno_width;
            int col_offset =
                editorRowRxToCx(&gCurFile->row[i], gCurFile->col_offset);
            int len = gCurFile->row[i].size - col_offset;
            len = (len < 0) ? 0 : len;

            int rlen = gCurFile->row[i].rsize - gCurFile->col_offset;
            is_row_full = (rlen > cols);
            rlen = is_row_full ? cols : rlen;
            rlen += gCurFile->col_offset;

            char* c = &gCurFile->row[i].data[col_offset];
            uint8_t* hl = &(gCurFile->row[i].hl[col_offset]);
            uint8_t curr_fg = HL_BG_NORMAL;
            uint8_t curr_bg = HL_NORMAL;

            setColor(ab, gEditor.color_cfg.highlightFg[curr_fg], 0);
            setColor(ab, gEditor.color_cfg.highlightBg[curr_bg], 1);

            int j = 0;
            int rx = gCurFile->col_offset;
            while (rx < rlen) {
                if (iscntrl(c[j]) && c[j] != '\t') {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abufAppend(ab, ANSI_INVERT);
                    abufAppendN(ab, &sym, 1);
                    abufAppend(ab, ANSI_CLEAR);
                    setColor(ab, gEditor.color_cfg.highlightFg[curr_fg], 0);
                    setColor(ab, gEditor.color_cfg.highlightBg[curr_bg], 1);

                    rx++;
                    j++;
                } else {
                    uint8_t fg = hl[j] & HL_FG_MASK;
                    uint8_t bg = hl[j] >> HL_FG_BITS;

                    if (gCurFile->cursor.is_selected &&
                        isPosSelected(i, j + col_offset, range)) {
                        bg = HL_BG_SELECT;
                    }
                    if (CONVAR_GETINT(drawspace) &&
                        (c[j] == ' ' || c[j] == '\t')) {
                        fg = HL_SPACE;
                    }
                    if (bg == HL_BG_TRAILING && !CONVAR_GETINT(trailing)) {
                        bg = HL_BG_NORMAL;
                    }

                    // Update color
                    if (fg != curr_fg) {
                        curr_fg = fg;
                        setColor(ab, gEditor.color_cfg.highlightFg[fg], 0);
                    }
                    if (bg != curr_bg) {
                        curr_bg = bg;
                        setColor(ab, gEditor.color_cfg.highlightBg[bg], 1);
                    }

                    if (c[j] == '\t') {
                        if (CONVAR_GETINT(drawspace)) {
                            abufAppend(ab, "|");
                        } else {
                            abufAppend(ab, " ");
                        }

                        rx++;
                        while (rx % CONVAR_GETINT(tabsize) != 0 && rx < rlen) {
                            abufAppend(ab, " ");
                            rx++;
                        }
                        j++;
                    } else if (c[j] == ' ') {
                        if (CONVAR_GETINT(drawspace)) {
                            abufAppend(ab, ".");
                        } else {
                            abufAppend(ab, " ");
                        }
                        rx++;
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
                setColor(ab, gEditor.color_cfg.highlightBg[HL_BG_SELECT], 1);
                abufAppend(ab, " ");
            }
            setColor(ab, gEditor.color_cfg.highlightBg[HL_BG_NORMAL], 1);
        }
        if (!is_row_full)
            abufAppend(ab, "\x1b[K");
        setColor(ab, gEditor.color_cfg.bg, 1);
    }
}

static void editorDrawFileExplorer(abuf* ab) {
    char* explorer_buf = malloc_s(gEditor.explorer.width + 1);
    gotoXY(ab, 1, 1);

    setColor(ab, gEditor.color_cfg.explorer[3], 0);
    if (gEditor.explorer.focused)
        setColor(ab, gEditor.color_cfg.explorer[4], 1);
    else
        setColor(ab, gEditor.color_cfg.explorer[0], 1);

    snprintf(explorer_buf, gEditor.explorer.width + 1, " EXPLORER%*s",
             gEditor.explorer.width, "");
    abufAppendN(ab, explorer_buf, gEditor.explorer.width);

    int lines = gEditor.explorer.flatten.size - gEditor.explorer.offset;
    if (lines < 0) {
        lines = 0;
    } else if (lines > gEditor.display_rows) {
        lines = gEditor.display_rows;
    }

    for (int i = 0; i < lines; i++) {
        gotoXY(ab, i + 2, 1);

        int index = gEditor.explorer.offset + i;
        EditorExplorerNode* node = gEditor.explorer.flatten.data[index];
        if (index == gEditor.explorer.selected_index)
            setColor(ab, gEditor.color_cfg.explorer[1], 1);
        else
            setColor(ab, gEditor.color_cfg.explorer[0], 1);

        const char* icon = "";
        if (node->is_directory) {
            setColor(ab, gEditor.color_cfg.explorer[2], 0);
            icon = node->is_open ? "v " : "> ";
        } else {
            setColor(ab, gEditor.color_cfg.explorer[3], 0);
        }
        const char* filename = getBaseName(node->filename);

        snprintf(explorer_buf, gEditor.explorer.width + 1, "%*s%s%s%*s",
                 node->depth * 2, "", icon, filename, gEditor.explorer.width,
                 "");
        abufAppendN(ab, explorer_buf, gEditor.explorer.width);
    }

    // Draw blank lines
    setColor(ab, gEditor.color_cfg.explorer[0], 1);
    setColor(ab, gEditor.color_cfg.explorer[3], 0);

    memset(explorer_buf, ' ', gEditor.explorer.width);

    for (int i = 0; i < gEditor.display_rows - lines; i++) {
        gotoXY(ab, lines + i + 2, 1);
        abufAppendN(ab, explorer_buf, gEditor.explorer.width);
    }

    free(explorer_buf);
}

void editorRefreshScreen(void) {
    abuf ab = ABUF_INIT;

    abufAppend(&ab, "\x1b[?25l");
    abufAppend(&ab, "\x1b[H");

    editorDrawTopStatusBar(&ab);
    editorDrawRows(&ab);
    editorDrawFileExplorer(&ab);

    if (gEditor.state != EDIT_MODE || *gEditor.status_msg[0] != '\0') {
        editorDrawPrompt(&ab);
    }

    editorDrawStatusBar(&ab);

    bool should_show_cursor = true;
    if (gEditor.state == EDIT_MODE) {
        int row = (gCurFile->cursor.y - gCurFile->row_offset) + 2;
        int col = (editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                   gCurFile->cursor.x) -
                   gCurFile->col_offset) +
                  1 + gCurFile->lineno_width;
        if (row <= 1 || row > gEditor.screen_rows - 1 || col <= 1 ||
            col > gEditor.screen_cols - gEditor.explorer.width ||
            (row == gEditor.screen_rows - 1 &&
             *gEditor.status_msg[0] != '\0')) {
            should_show_cursor = false;
        } else {
            gotoXY(&ab, row, col + gEditor.explorer.width);
        }
    } else {
        // prompt
        gotoXY(&ab, gEditor.screen_rows - 1, gEditor.px + 1);
    }

    if (gEditor.explorer.focused) {
        should_show_cursor = false;
    }

    if (should_show_cursor) {
        abufAppend(&ab, "\x1b[?25h");
    } else {
        abufAppend(&ab, "\x1b[?25l");
    }

    UNUSED(write(STDOUT_FILENO, ab.buf, ab.len));
    abufFree(&ab);
}
