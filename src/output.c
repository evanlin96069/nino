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
#include "prompt.h"
#include "select.h"
#include "unicode.h"
#include "version.h"

static void editorDrawTopStatusBar(abuf* ab) {
    const char* right_buf = "  nino v" EDITOR_VERSION " ";
    bool has_more_files = false;
    int rlen = strlen(right_buf);
    int len = gEditor.explorer.width;

    gotoXY(ab, 1, gEditor.explorer.width + 1);

    setColor(ab, gEditor.color_cfg.top_status[0], 0);
    setColor(ab, gEditor.color_cfg.top_status[1], 1);

    if (gEditor.tab_offset != 0) {
        abufAppendN(ab, "<", 1);
        len++;
    }

    gEditor.tab_displayed = 0;
    if (gEditor.loading) {
        const char* loading_text = "Loading...";
        int loading_text_len = strlen(loading_text);
        abufAppendN(ab, loading_text, loading_text_len);
        len = loading_text_len;
    } else {
        for (int i = 0; i < gEditor.file_count; i++) {
            if (i < gEditor.tab_offset)
                continue;

            const EditorFile* file = &gEditor.files[i];

            bool is_current = (file == gCurFile);
            if (is_current) {
                setColor(ab, gEditor.color_cfg.top_status[4], 0);
                setColor(ab, gEditor.color_cfg.top_status[5], 1);
            } else {
                setColor(ab, gEditor.color_cfg.top_status[2], 0);
                setColor(ab, gEditor.color_cfg.top_status[3], 1);
            }

            char buf[EDITOR_PATH_MAX] = {0};
            const char* filename =
                file->filename ? getBaseName(file->filename) : "Untitled";
            int buf_len = snprintf(buf, sizeof(buf), " %s%s ",
                                   file->dirty ? "*" : "", filename);
            int tab_width = strUTF8Width(buf);

            if (gEditor.screen_cols - len < tab_width ||
                (i != gEditor.file_count - 1 &&
                 gEditor.screen_cols - len == tab_width)) {
                has_more_files = true;
                if (gEditor.tab_displayed == 0) {
                    // Display at least one tab
                    // TODO: This is wrong
                    tab_width = gEditor.screen_cols - len - 1;
                    buf_len = gEditor.screen_cols - len - 1;
                } else {
                    break;
                }
            }

            // Not enough space to even show one tab
            if (tab_width < 0)
                break;

            abufAppendN(ab, buf, buf_len);
            len += tab_width;
            gEditor.tab_displayed++;
        }
    }

    setColor(ab, gEditor.color_cfg.top_status[0], 0);
    setColor(ab, gEditor.color_cfg.top_status[1], 1);

    if (has_more_files) {
        abufAppendN(ab, ">", 1);
        len++;
    }

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            abufAppendN(ab, right_buf, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}

static void editorDrawConMsg(abuf* ab) {
    if (gEditor.con_size == 0) {
        return;
    }

    setColor(ab, gEditor.color_cfg.prompt[0], 0);
    setColor(ab, gEditor.color_cfg.prompt[1], 1);

    bool should_draw_prompt =
        (gEditor.state != EDIT_MODE && gEditor.state != EXPLORER_MODE);
    int draw_x = gEditor.screen_rows - gEditor.con_size;
    if (should_draw_prompt) {
        draw_x--;
    }

    int index = gEditor.con_front;
    for (int i = 0; i < gEditor.con_size; i++) {
        gotoXY(ab, draw_x, 0);
        draw_x++;

        const char* buf = gEditor.con_msg[index];
        index = (index + 1) % EDITOR_CON_COUNT;

        int len = strlen(buf);
        if (len > gEditor.screen_cols) {
            len = gEditor.screen_cols;
        }

        abufAppendN(ab, buf, len);

        while (len < gEditor.screen_cols) {
            abufAppend(ab, " ");
            len++;
        }
    }
}

static void editorDrawPrompt(abuf* ab) {
    bool should_draw_prompt =
        (gEditor.state != EDIT_MODE && gEditor.state != EXPLORER_MODE);
    if (!should_draw_prompt) {
        return;
    }

    setColor(ab, gEditor.color_cfg.prompt[0], 0);
    setColor(ab, gEditor.color_cfg.prompt[1], 1);

    gotoXY(ab, gEditor.screen_rows - 1, 0);

    const char* left = gEditor.prompt;
    int len = strlen(left);

    const char* right = gEditor.prompt_right;
    int rlen = strlen(right);

    if (rlen > gEditor.screen_cols) {
        rlen = 0;
    }

    if (len + rlen > gEditor.screen_cols) {
        len = gEditor.screen_cols - rlen;
    }

    abufAppendN(ab, left, len);

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            abufAppendN(ab, right, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}

static void editorDrawStatusBar(abuf* ab) {
    gotoXY(ab, gEditor.screen_rows, 0);

    setColor(ab, gEditor.color_cfg.status[0], 0);
    setColor(ab, gEditor.color_cfg.status[1], 1);

    const char* help_str = "";
    const char* help_info[] = {
        " ^Q: Quit  ^O: Open  ^P: Prompt  ^S: Save  ^F: Find  ^G: Goto",
        " ^Q: Quit  ^O: Open  ^P: Prompt",
        " ^Q: Cancel  Up: back  Down: Next",
        " ^Q: Cancel",
        " ^Q: Cancel",
        " ^Q: Cancel",
        " ^Q: Cancel",
    };
    if (CONVAR_GETINT(helpinfo))
        help_str = help_info[gEditor.state];

    char lang[16];
    char pos[64];
    int len = strlen(help_str);
    int lang_len, pos_len;
    int rlen;
    if (gEditor.file_count == 0) {
        lang_len = 0;
        pos_len = 0;
    } else {
        const char* file_type =
            gCurFile->syntax ? gCurFile->syntax->file_type : "Plain Text";
        int row = gCurFile->cursor.y + 1;
        int col = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                  gCurFile->cursor.x) +
                  1;
        float line_percent = 0.0f;
        const char* nl_type = (gCurFile->newline == NL_UNIX) ? "LF" : "CRLF";
        if (gCurFile->num_rows - 1 > 0) {
            line_percent =
                (float)gCurFile->row_offset / (gCurFile->num_rows - 1) * 100.0f;
        }

        lang_len = snprintf(lang, sizeof(lang), "  %s  ", file_type);
        pos_len = snprintf(pos, sizeof(pos), " %d:%d [%.f%%] <%s> ", row, col,
                           line_percent, nl_type);
    }

    rlen = lang_len + pos_len;

    if (rlen > gEditor.screen_cols)
        rlen = 0;
    if (len + rlen > gEditor.screen_cols)
        len = gEditor.screen_cols - rlen;

    abufAppendN(ab, help_str, len);

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            setColor(ab, gEditor.color_cfg.status[2], 0);
            setColor(ab, gEditor.color_cfg.status[3], 1);
            abufAppendN(ab, lang, lang_len);
            setColor(ab, gEditor.color_cfg.status[4], 0);
            setColor(ab, gEditor.color_cfg.status[5], 1);
            abufAppendN(ab, pos, pos_len);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}

static void editorDrawRows(abuf* ab) {
    setColor(ab, gEditor.color_cfg.bg, 1);

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
    if (gEditor.state == EXPLORER_MODE)
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

    editorDrawConMsg(&ab);
    editorDrawPrompt(&ab);

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
            row >= gEditor.screen_rows - gEditor.con_size) {
            should_show_cursor = false;
        } else {
            gotoXY(&ab, row, col + gEditor.explorer.width);
        }
    } else {
        // prompt
        gotoXY(&ab, gEditor.screen_rows - 1, gEditor.px + 1);
    }

    if (gEditor.state == EXPLORER_MODE) {
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
