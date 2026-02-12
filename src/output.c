#include "output.h"

#include <ctype.h>

#include "config.h"
#include "editor.h"
#include "highlight.h"
#include "os.h"
#include "select.h"
#include "terminal.h"
#include "unicode.h"
#include "utils.h"

static inline bool styleEql(const ScreenStyle* a, const ScreenStyle* b) {
    return colorEql(a->fg, b->fg) && colorEql(a->bg, b->bg);
}

static inline bool graphemeEql(const Grapheme* a, const Grapheme* b) {
    if (a->size != b->size)
        return false;

    for (int i = 0; i < a->size; i++) {
        if (a->cluster[i] != b->cluster[i]) {
            return false;
        }
    }
    return true;
}

static bool cellEql(const ScreenCell* a, const ScreenCell* b) {
    if (a->continuation != b->continuation)
        return false;
    if (a->continuation)
        return true;

    if (!graphemeEql(&a->grapheme, &b->grapheme))
        return false;

    return styleEql(&a->style, &b->style);
}

static bool editorScreenRowUpdated(int index) {
    const ScreenCell* row = gEditor.screen[index];
    const ScreenCell* prev_row = gEditor.prev_screen[index];
    for (int i = 0; i < gEditor.screen_cols; i++) {
        if (!cellEql(&row[i], &prev_row[i])) {
            return true;
        }
    }
    return false;
}

static void updateStyle(abuf* ab,
                        const ScreenStyle* old_style,
                        const ScreenStyle* new_style) {
    if (!new_style)
        return;

    bool update_fg = (!old_style || !colorEql(old_style->fg, new_style->fg));
    bool update_bg = (!old_style || !colorEql(old_style->bg, new_style->bg));
    if (update_fg && update_bg) {
        setColors(ab, new_style->fg, new_style->bg);
    } else if (update_fg) {
        setColor(ab, new_style->fg, false);
    } else if (update_bg) {
        setColor(ab, new_style->bg, true);
    }
}

static Grapheme grapheme_space = {
    .cluster = {[0] = ' '},
    .size = 1,
    .width = 1,
};

static void editorRenderRow(abuf* ab, int row_index) {
    ScreenCell* row = gEditor.screen[row_index];

    const ScreenStyle* old_style = NULL;

    gotoXY(ab, row_index + 1, 1);

    int index = 0;
    while (index < gEditor.screen_cols) {
        ScreenCell* cell = &row[index];
        Grapheme grapheme = cell->grapheme;

        updateStyle(ab, old_style, &cell->style);
        old_style = &cell->style;

        if (cell->continuation || grapheme.size == 0 || grapheme.width == 0) {
            // These are not supposed to happen
            // Default to white space
            grapheme = grapheme_space;
        }

        char output[4];
        int utf8_len = encodeUTF8(grapheme.cluster[0], output);
        if (utf8_len == -1) {
            // Replace with the replacement character
            grapheme.cluster[0] = 0xFFFD;
            grapheme.size = 1;
            grapheme.width = 1;
            utf8_len = encodeUTF8(grapheme.cluster[0], output);
        }

        // Check if this character fits
        bool canDraw = true;
        int offset = 1;
        while (offset < grapheme.width) {
            if (index + offset >= gEditor.screen_cols ||
                !row[index + offset].continuation) {
                canDraw = false;
                break;
            }
            offset++;
        }

        index += offset;

        if (!canDraw) {
            // Draw spaces until filling the character width we can draw
            output[0] = ' ';
            for (int j = 0; j < offset; j++) {
                abufAppendN(ab, output, 1);
            }
        } else {
            abufAppendN(ab, output, (size_t)utf8_len);
            for (int i = 1; i < grapheme.size; i++) {
                utf8_len = encodeUTF8(grapheme.cluster[i], output);
                if (utf8_len != -1) {
                    abufAppendN(ab, output, (size_t)utf8_len);
                }
            }
        }
    }
}

static void screenClearCells(ScreenCell* row,
                             int max_width,
                             int x,
                             int count,
                             ScreenStyle style) {
    if (x >= max_width)
        return;

    int to_clear = count;
    if (x + count > max_width) {
        to_clear = max_width - x;
    }

    for (int i = 0; i < to_clear; i++) {
        row[x + i].continuation = false;
        row[x + i].grapheme = grapheme_space;
        row[x + i].style = style;
    }
}

// Put a grapheme in a cell
// Marks the cells occupied by the grapheme as continuation
// Returns the number of cells used (grapheme width)
static int screenPutGrapheme(ScreenCell* row,
                             int max_width,
                             int x,
                             const Grapheme* grapheme,
                             const ScreenStyle* style) {
    if (x >= max_width || grapheme->width <= 0)
        return 0;

    int width = grapheme->width;
    if (x + width > max_width)
        width = max_width - x;

    // Set the first cell
    row[x].continuation = false;
    row[x].grapheme = *grapheme;
    row[x].style = *style;

    // Mark continuation cells
    for (int i = 1; i < width; i++) {
        row[x + i].continuation = true;
        row[x + i].style = *style;
    }

    return width;
}

static int screenPutChar(ScreenCell* row,
                         int max_width,
                         int x,
                         uint32_t code_point,
                         const ScreenStyle* style) {
    int width = unicodeWidth(code_point);
    if (width < 0 || x >= max_width)
        return 0;

    Grapheme grapheme = {0};
    grapheme.cluster[0] = code_point;
    grapheme.size = 1;
    grapheme.width = width;

    return screenPutGrapheme(row, max_width, x, &grapheme, style);
}

static int screenPutUtf8(ScreenCell* row,
                         int max_width,
                         int x,
                         const char* s,
                         const ScreenStyle style) {
    if (!s)
        return 0;

    int start_x = x;
    size_t len = strlen(s);
    const char* p = s;

    while (*p != '\0' && x < max_width) {
        // Skip zero-width characters until we find a base character (width > 0)
        size_t byte_size;
        uint32_t code_point = decodeUTF8(p, len, &byte_size);

        if (byte_size == 0)
            break;

        int width = unicodeWidth(code_point);
        if (width <= 0) {
            // Invalid or zero-width character
            p += byte_size;
            len -= byte_size;
            continue;
        }

        if (x + width > max_width)
            break;

        // Build grapheme cluster
        Grapheme grapheme = {0};
        grapheme.cluster[0] = code_point;
        grapheme.size = 1;
        grapheme.width = width;

        p += byte_size;
        len -= byte_size;

        // Add following zero-width characters
        while (grapheme.size < MAX_CLUSTER_SIZE && *p != '\0' && len > 0) {
            size_t comb_byte_size;
            uint32_t comb_code_point = decodeUTF8(p, len, &comb_byte_size);

            if (comb_byte_size == 0)
                break;

            int comb_width = unicodeWidth(comb_code_point);
            if (comb_width < 0) {
                // Invalid character
                p += comb_byte_size;
                len -= comb_byte_size;
                break;
            }

            if (comb_width > 0)
                break;

            grapheme.cluster[grapheme.size] = comb_code_point;
            grapheme.size++;

            p += comb_byte_size;
            len -= comb_byte_size;
        }

        x += screenPutGrapheme(row, max_width, x, &grapheme, &style);
    }

    return x - start_x;
}

static int screenPutAscii(ScreenCell* row,
                          int max_width,
                          int x,
                          const char* s,
                          const ScreenStyle style) {
    if (!s)
        return 0;

    int start_x = x;
    const char* p = s;

    while (*p != '\0' && x < max_width) {
        unsigned char c = (unsigned char)*p;

        // Fast path: ASCII characters are always width 1
        Grapheme grapheme = {0};
        grapheme.cluster[0] = c;
        grapheme.size = 1;
        grapheme.width = 1;

        x += screenPutGrapheme(row, max_width, x, &grapheme, &style);
        p++;
    }

    return x - start_x;
}

static void editorDrawTopStatusBar(void) {
    if (gEditor.explorer.width >= gEditor.screen_cols) {
        return;
    }

    ScreenCell* row = gEditor.screen[0];
    ScreenStyle default_style = {
        .fg = gEditor.color_cfg.top_status[0],
        .bg = gEditor.color_cfg.top_status[1],
    };

    screenClearCells(row, gEditor.screen_cols, gEditor.explorer.width,
                     gEditor.screen_cols - gEditor.explorer.width,
                     default_style);

    const char* right_buf = "  " EDITOR_NAME " v" EDITOR_VERSION " ";
    int rlen = strlen(right_buf);
    int x = gEditor.explorer.width;

    ScreenStyle style = default_style;

    if (gEditor.tab_offset != 0) {
        screenPutAscii(row, gEditor.screen_cols, x, "<", style);
        x++;
    }

    gEditor.tab_displayed = 0;
    if (gEditor.state == LOADING_MODE) {
        const char* loading_text = "Loading...";
        x += screenPutAscii(row, gEditor.screen_cols, x, loading_text, style);
    } else {
        for (int i = 0; i < gEditor.file_count; i++) {
            if (i < gEditor.tab_offset)
                continue;

            int remaining_width = gEditor.screen_cols - x;
            if (remaining_width < 0)
                break;

            const EditorFile* file = &gEditor.files[i];

            bool is_current = (file == gCurFile);
            if (is_current) {
                style.fg = gEditor.color_cfg.top_status[4];
                style.bg = gEditor.color_cfg.top_status[5];
            } else {
                style.fg = gEditor.color_cfg.top_status[2];
                style.bg = gEditor.color_cfg.top_status[3];
            }

            char buf[EDITOR_PATH_MAX] = {0};
            if (file->filename) {
                const char* basename = getBaseName(file->filename);
                snprintf(buf, sizeof(buf), " %s%s ", file->dirty ? "*" : "",
                         basename);
            } else {
                snprintf(buf, sizeof(buf), " Untitled-%d%s ", file->new_id + 1,
                         file->dirty ? "*" : "");
            }

            int tab_width = strUTF8Width(buf);

            if (remaining_width < tab_width ||
                (i != gEditor.file_count - 1 && remaining_width == tab_width)) {
                if (gEditor.tab_displayed == 0) {
                    // Display at least one tab (truncated if needed)
                    if (remaining_width > 1) {
                        x += screenPutUtf8(row, gEditor.screen_cols, x, buf,
                                           style);
                        gEditor.tab_displayed++;
                    }
                } else {
                    break;
                }
            } else {
                // Not enough space to even show one tab
                if (remaining_width <= 0)
                    break;

                x += screenPutUtf8(row, gEditor.screen_cols, x, buf, style);
                gEditor.tab_displayed++;
            }
        }

        style = default_style;

        if (gEditor.tab_offset + gEditor.tab_displayed < gEditor.file_count) {
            if (x >= gEditor.screen_cols) {
                screenPutAscii(row, gEditor.screen_cols,
                               gEditor.screen_cols - 1, ">", style);
                x = gEditor.screen_cols;
            } else {
                screenPutAscii(row, gEditor.screen_cols, x, ">", style);
                x++;
            }
        }
    }

    if (gEditor.screen_cols - x >= rlen && rlen > 0) {
        screenPutAscii(row, gEditor.screen_cols, gEditor.screen_cols - rlen,
                       right_buf, style);
    }
}

static void editorDrawConMsg(void) {
    if (gEditor.con_size == 0) {
        return;
    }

    ScreenStyle style = {
        .fg = gEditor.color_cfg.prompt[0],
        .bg = gEditor.color_cfg.prompt[1],
    };

    // con_size + status bar
    int draw_row = gEditor.screen_rows - (gEditor.con_size + 1);

    bool should_draw_prompt =
        (gEditor.state != EDIT_MODE && gEditor.state != EXPLORER_MODE);
    if (should_draw_prompt) {
        draw_row--;
    }

    int index = gEditor.con_front;
    for (int i = 0; i < gEditor.con_size; i++) {
        ScreenCell* row = gEditor.screen[draw_row];
        screenClearCells(row, gEditor.screen_cols, 0, gEditor.screen_cols,
                         style);

        const char* buf = gEditor.con_msg[index];
        index = (index + 1) % EDITOR_CON_COUNT;

        screenPutUtf8(row, gEditor.screen_cols, 0, buf, style);

        draw_row++;
    }
}

static void editorDrawPrompt(void) {
    bool should_draw_prompt =
        (gEditor.state != EDIT_MODE && gEditor.state != EXPLORER_MODE);
    if (!should_draw_prompt) {
        return;
    }

    ScreenCell* row =
        gEditor.screen[gEditor.screen_rows - 2];  // prompt + status bar
    ScreenStyle style = {
        .fg = gEditor.color_cfg.prompt[0],
        .bg = gEditor.color_cfg.prompt[1],
    };

    screenClearCells(row, gEditor.screen_cols, 0, gEditor.screen_cols, style);

    const char* left = gEditor.prompt;
    const char* right = gEditor.prompt_right;

    // Right prompt is currently only used by find mode, assume it's ASCII
    int rlen = strlen(right);
    if (rlen > gEditor.screen_cols) {
        rlen = 0;
    }

    int x = screenPutUtf8(row, gEditor.screen_cols, 0, left, style);
    if (x < gEditor.screen_cols - rlen) {
        screenPutAscii(row, gEditor.screen_cols, gEditor.screen_cols - rlen,
                       right, style);
    }
}

static void editorDrawStatusBar(void) {
    ScreenCell* row = gEditor.screen[gEditor.screen_rows - 1];
    ScreenStyle default_style = {
        .fg = gEditor.color_cfg.status[0],
        .bg = gEditor.color_cfg.status[1],
    };

    screenClearCells(row, gEditor.screen_cols, 0, gEditor.screen_cols,
                     default_style);

    const char* help_str = "";
    const char* help_info[] = {
        [LOADING_MODE] = "",
        [EDIT_MODE] =
            " ^Q: Quit  ^O: Open  ^P: Prompt  ^S: Save  ^F: Find  ^G: Goto",
        [EXPLORER_MODE] = " ^Q: Quit  ^O: Open  ^P: Prompt",
        [FIND_MODE] = " ^Q: Cancel  Up: Back  Down: Next",
        [GOTO_LINE_MODE] = " ^Q: Cancel",
        [OPEN_FILE_MODE] = " ^Q: Cancel",
        [CONFIG_MODE] = " ^Q: Cancel",
        [SAVE_AS_MODE] = " ^Q: Cancel",
    };
    if (CONVAR_GETINT(helpinfo))
        help_str = help_info[gEditor.state];

    char lang[16];
    char pos[64];
    int rlen;
    if (gEditor.file_count == 0) {
        rlen = 0;
    } else {
        const char* file_type =
            gCurFile->syntax ? gCurFile->syntax->file_type : "Plain Text";
        int row_num = gCurFile->cursor.y + 1;
        int col = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                  gCurFile->cursor.x) +
                  1;
        float line_percent = 0.0f;
        const char* nl_type = (gCurFile->newline == NL_UNIX) ? "LF" : "CRLF";
        if (gCurFile->num_rows - 1 > 0) {
            line_percent =
                (float)gCurFile->row_offset / (gCurFile->num_rows - 1) * 100.0f;
        }

        snprintf(lang, sizeof(lang), "  %s  ", file_type);
        snprintf(pos, sizeof(pos), " %d:%d [%.f%%] <%s> ", row_num, col,
                 line_percent, nl_type);
        rlen = strUTF8Width(lang) + strUTF8Width(pos);
    }

    if (rlen > gEditor.screen_cols)
        rlen = 0;

    int x = 0;
    ScreenStyle style = default_style;

    int max_help_width =
        (rlen > 0) ? gEditor.screen_cols - rlen : gEditor.screen_cols;
    if (max_help_width > 0) {
        x += screenPutAscii(row, max_help_width, 0, help_str, style);
    }

    if (rlen > 0 && gEditor.screen_cols - x >= rlen) {
        int right_x = gEditor.screen_cols - rlen;
        style.fg = gEditor.color_cfg.status[2];
        style.bg = gEditor.color_cfg.status[3];
        int lang_width = strUTF8Width(lang);
        screenPutAscii(row, gEditor.screen_cols, right_x, lang, style);
        style.fg = gEditor.color_cfg.status[4];
        style.bg = gEditor.color_cfg.status[5];
        screenPutAscii(row, gEditor.screen_cols, right_x + lang_width, pos,
                       style);
    }
}

static void editorDrawRows(void) {
    if (gEditor.explorer.width >= gEditor.screen_cols) {
        return;
    }

    EditorSelectRange range = {0};
    if (gCurFile->cursor.is_selected)
        getSelectStartEnd(&gCurFile->cursor, &range);

    int lineno_width = LINENO_WIDTH();
    int content_start_col = gEditor.explorer.width + lineno_width;
    int content_cols = gEditor.screen_cols - content_start_col;

    for (int i = gCurFile->row_offset, s_row = 1;
         i < gCurFile->row_offset + gEditor.display_rows; i++, s_row++) {
        ScreenCell* row = gEditor.screen[s_row];

        // Clear the entire row
        ScreenStyle bg_style = {
            .fg = gEditor.color_cfg.highlightFg[HL_NORMAL],
            .bg = gEditor.color_cfg.bg,
        };
        if (i == gCurFile->cursor.y && !gCurFile->cursor.is_selected) {
            bg_style.bg = gEditor.color_cfg.cursor_line;
        }
        screenClearCells(row, gEditor.screen_cols, gEditor.explorer.width,
                         gEditor.screen_cols - gEditor.explorer.width,
                         bg_style);

        if (i < gCurFile->num_rows) {
            int x = gEditor.explorer.width;

            // Draw line number
            if (CONVAR_GETINT(lineno)) {
                ScreenStyle lineno_style;
                lineno_style.fg = (i == gCurFile->cursor.y)
                                      ? gEditor.color_cfg.line_number[1]
                                      : gEditor.color_cfg.line_number[0];
                lineno_style.bg = (i == gCurFile->cursor.y)
                                      ? gEditor.color_cfg.line_number[0]
                                      : gEditor.color_cfg.line_number[1];

                char line_number[16];
                snprintf(line_number, sizeof(line_number), " %*d ",
                         gCurFile->lineno_width - 2, i + 1);
                x += screenPutAscii(row, gEditor.screen_cols, x, line_number,
                                    lineno_style);
            }

            // Draw content
            int col_offset =
                editorRowRxToCx(&gCurFile->row[i], gCurFile->col_offset);
            int data_len = gCurFile->row[i].size - col_offset;
            if (data_len < 0) {
                data_len = 0;
            }

            int rlen = gCurFile->row[i].rsize - gCurFile->col_offset;
            if (rlen > content_cols) {
                rlen = content_cols;
            }
            rlen += gCurFile->col_offset;

            char* c = &gCurFile->row[i].data[col_offset];
            uint8_t* hl = &(gCurFile->row[i].hl[col_offset]);

            int j = 0;
            int rx = gCurFile->col_offset;
            int screen_x = content_start_col;

            Grapheme* curr_grapheme = NULL;

            while (rx < rlen && screen_x < gEditor.screen_cols) {
                uint8_t fg = hl[j] & HL_FG_MASK;
                uint8_t bg = hl[j] >> HL_FG_BITS;
                if (gCurFile->cursor.is_selected &&
                    isPosSelected(i, j + col_offset, range)) {
                    bg = HL_BG_SELECT;
                }

                ScreenStyle style = {0};

                if (iscntrl((uint8_t)c[j]) && c[j] != '\t') {
                    // Control character (show inverted)
                    style.fg = gEditor.color_cfg.highlightFg[fg];
                    if (bg == HL_BG_NORMAL) {
                        style.bg = bg_style.bg;
                    } else {
                        style.bg = gEditor.color_cfg.highlightBg[bg];
                    }

                    uint32_t sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    Color tmp = style.fg;
                    style.fg = style.bg;
                    style.bg = tmp;

                    screen_x += screenPutChar(row, gEditor.screen_cols,
                                              screen_x, sym, &style);
                    curr_grapheme = NULL;
                    rx++;
                    j++;
                } else {
                    if (CONVAR_GETINT(drawspace) &&
                        (c[j] == ' ' || c[j] == '\t')) {
                        fg = HL_SPACE;
                    }
                    if (bg == HL_BG_TRAILING && !CONVAR_GETINT(trailing)) {
                        bg = HL_BG_NORMAL;
                    }

                    style.fg = gEditor.color_cfg.highlightFg[fg];
                    if (bg == HL_BG_NORMAL) {
                        style.bg = bg_style.bg;
                    } else {
                        style.bg = gEditor.color_cfg.highlightBg[bg];
                    }

                    if (c[j] == '\t') {
                        char tab_char = CONVAR_GETINT(drawspace) ? '|' : ' ';
                        screen_x += screenPutChar(row, gEditor.screen_cols,
                                                  screen_x, tab_char, &style);
                        rx++;
                        while (rx % CONVAR_GETINT(tabsize) != 0 && rx < rlen &&
                               screen_x < gEditor.screen_cols) {
                            screen_x += screenPutChar(row, gEditor.screen_cols,
                                                      screen_x, ' ', &style);
                            rx++;
                        }
                        curr_grapheme = NULL;
                        j++;
                    } else if (c[j] == ' ') {
                        char space_char = CONVAR_GETINT(drawspace) ? '.' : ' ';
                        screen_x += screenPutChar(row, gEditor.screen_cols,
                                                  screen_x, space_char, &style);
                        curr_grapheme = NULL;
                        rx++;
                        j++;
                    } else {
                        size_t byte_size;
                        uint32_t unicode =
                            decodeUTF8(&c[j], data_len - j, &byte_size);
                        int width = unicodeWidth(unicode);
                        if (width < 0) {
                            unicode = 0xFFFD;
                            width = 1;
                        }

                        if (width == 0) {
                            if (curr_grapheme &&
                                curr_grapheme->size < MAX_CLUSTER_SIZE) {
                                curr_grapheme->cluster[curr_grapheme->size] =
                                    unicode;
                                curr_grapheme->size++;
                            }
                        } else {
                            curr_grapheme = &row[screen_x].grapheme;
                            screen_x +=
                                screenPutChar(row, gEditor.screen_cols,
                                              screen_x, unicode, &style);
                            rx += width;
                        }
                        j += byte_size;
                    }
                }

                // Gather trailing zero-width characters
                while (data_len - j > 0) {
                    size_t byte_size;
                    uint32_t unicode =
                        decodeUTF8(&c[j], data_len - j, &byte_size);
                    int width = unicodeWidth(unicode);
                    if (width != 0)
                        break;
                    if (curr_grapheme &&
                        curr_grapheme->size < MAX_CLUSTER_SIZE) {
                        curr_grapheme->cluster[curr_grapheme->size] = unicode;
                        curr_grapheme->size++;
                    }
                    j += byte_size;
                }
            }

            // Add newline character when selected
            if (gCurFile->cursor.is_selected && range.end_y > i &&
                i >= range.start_y &&
                gCurFile->row[i].rsize - gCurFile->col_offset < content_cols &&
                screen_x < gEditor.screen_cols) {
                ScreenStyle select_style = {
                    .fg = gEditor.color_cfg.highlightFg[HL_BG_NORMAL],
                    .bg = gEditor.color_cfg.highlightBg[HL_BG_SELECT],
                };
                screenPutChar(row, gEditor.screen_cols, screen_x, ' ',
                              &select_style);
            }
        }
    }
}

static void editorDrawFileExplorer(void) {
    if (gEditor.explorer.width == 0) {
        return;
    }

    int explorer_width = gEditor.explorer.width;
    if (explorer_width > gEditor.screen_cols) {
        explorer_width = gEditor.screen_cols;
    }

    // Draw header
    ScreenCell* header_row = gEditor.screen[0];
    ScreenStyle header_style = {
        .fg = gEditor.color_cfg.explorer[3],
        .bg = (gEditor.state == EXPLORER_MODE) ? gEditor.color_cfg.explorer[4]
                                               : gEditor.color_cfg.explorer[0],
    };

    screenClearCells(header_row, gEditor.screen_cols, 0, explorer_width,
                     header_style);
    screenPutAscii(header_row, explorer_width, 0, " EXPLORER", header_style);

    int lines = gEditor.explorer.flatten.size - gEditor.explorer.offset;
    if (lines < 0) {
        lines = 0;
    } else if (lines > gEditor.display_rows) {
        lines = gEditor.display_rows;
    }

    ScreenStyle default_style = {
        .fg = gEditor.color_cfg.explorer[3],
        .bg = gEditor.color_cfg.explorer[0],
    };
    ScreenStyle directory_style = {
        .fg = gEditor.color_cfg.explorer[2],
        .bg = gEditor.color_cfg.explorer[0],
    };

    for (int i = 0; i < lines; i++) {
        ScreenCell* row = gEditor.screen[i + 1];  // Row 1 to display_rows+1
        int index = gEditor.explorer.offset + i;
        EditorExplorerNode* node = gEditor.explorer.flatten.data[index];

        ScreenStyle row_style =
            (node->is_directory) ? directory_style : default_style;
        if (index == gEditor.explorer.selected_index) {
            row_style.bg = gEditor.color_cfg.explorer[1];
        }

        screenClearCells(row, gEditor.screen_cols, 0, explorer_width,
                         row_style);

        // Indentation
        int x = node->depth * 2;

        if (node->is_directory) {
            const char* icon = node->is_open ? "v " : "> ";
            x += screenPutAscii(row, explorer_width, x, icon, row_style);
        }

        const char* filename = getBaseName(node->filename);
        screenPutUtf8(row, explorer_width, x, filename, row_style);
    }

    // Draw blank lines
    for (int i = 0; i < gEditor.display_rows - lines; i++) {
        ScreenCell* row = gEditor.screen[lines + i + 1];
        screenClearCells(row, gEditor.screen_cols, 0, explorer_width,
                         default_style);
    }
}

void editorRefreshScreen(void) {
    if (gEditor.screen_size_updated) {
        if (gEditor.screen) {
            for (int i = 0; i < gEditor.old_screen_rows; i++) {
                free(gEditor.screen[i]);
            }
            free(gEditor.screen);
        }
        if (gEditor.prev_screen) {
            for (int i = 0; i < gEditor.old_screen_rows; i++) {
                free(gEditor.prev_screen[i]);
            }
            free(gEditor.prev_screen);
        }

        gEditor.screen = malloc_s(gEditor.screen_rows * sizeof(ScreenCell*));
        for (int i = 0; i < gEditor.screen_rows; i++) {
            gEditor.screen[i] =
                malloc_s(gEditor.screen_cols * sizeof(ScreenCell));
        }
        gEditor.prev_screen =
            malloc_s(gEditor.screen_rows * sizeof(ScreenCell*));
        for (int i = 0; i < gEditor.screen_rows; i++) {
            gEditor.prev_screen[i] =
                malloc_s(gEditor.screen_cols * sizeof(ScreenCell));
        }
    }

    abuf ab = ABUF_INIT;

    abufAppendStr(&ab, ANSI_CURSOR_HIDE ANSI_CURSOR_RESET_POS);

    // Draw screen
    editorDrawTopStatusBar();
    editorDrawRows();
    editorDrawFileExplorer();

    editorDrawConMsg();
    editorDrawPrompt();

    editorDrawStatusBar();

    // Render sreen
    for (int i = 0; i < gEditor.screen_rows; i++) {
        if (gEditor.screen_size_updated || editorScreenRowUpdated(i)) {
            editorRenderRow(&ab, i);
            // Save current screen
            memcpy(gEditor.prev_screen[i], gEditor.screen[i],
                   sizeof(ScreenCell) * gEditor.screen_cols);
        }
    }
    gEditor.screen_size_updated = false;

    // Crosshair
    bool should_show_cursor = false;
    switch (gEditor.state) {
        case EDIT_MODE: {
            int row = (gCurFile->cursor.y - gCurFile->row_offset) + 2;
            int col = (editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                       gCurFile->cursor.x) -
                       gCurFile->col_offset) +
                      1 + LINENO_WIDTH();
            if (row <= 1 || row > gEditor.screen_rows - 1 || col <= 0 ||
                col > gEditor.screen_cols - gEditor.explorer.width ||
                row >= gEditor.screen_rows - gEditor.con_size) {
                should_show_cursor = false;
            } else {
                should_show_cursor = true;
                gotoXY(&ab, row, col + gEditor.explorer.width);
            }
        } break;

        case LOADING_MODE:
        case EXPLORER_MODE:
            should_show_cursor = false;
            break;

        default:
            // prompt
            should_show_cursor = true;
            gotoXY(&ab, gEditor.screen_rows - 1, gEditor.px + 1);
    }

    if (should_show_cursor) {
        abufAppendStr(&ab, ANSI_CURSOR_SHOW);
    }

    abufAppendStr(&ab, ANSI_CLEAR);

    writeConsoleAll(ab.buf, ab.len);
    abufFree(&ab);
}
