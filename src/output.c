#include "output.h"

#include "config.h"
#include "editor.h"
#include "highlight.h"
#include "os.h"
#include "select.h"
#include "terminal.h"
#include "unicode.h"
#include "utils.h"

#include "panels/edit.h"
#include "ui/panel.h"
#include "ui/surface.h"

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

static bool editorScreenRowUpdated(int index, int* start_col, int* end_col) {
    const ScreenCell* row = &SURFACE_AT(gEditor.screen, 0, index);
    const ScreenCell* old_row = &SURFACE_AT(gEditor.old_screen, 0, index);

    int start = -1;
    int end = -1;

    for (int i = 0; i < gEditor.screen_cols; i++) {
        if (!cellEql(&row[i], &old_row[i])) {
            start = i;
            break;
        }
    }
    for (int i = gEditor.screen_cols - 1; i > start; i--) {
        if (!cellEql(&row[i], &old_row[i])) {
            end = i;
            break;
        }
    }
    if (end == -1) {
        end = start;
    }

    if (start_col)
        *start_col = start;
    if (end_col)
        *end_col = end;

    return start != -1 && end != -1;
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

static void editorRenderRow(abuf* ab,
                            int row_index,
                            int start_col,
                            int end_col) {
    ScreenCell* row = &SURFACE_AT(gEditor.screen, 0, row_index);

    const ScreenStyle* old_style = NULL;

    gotoXY(ab, row_index + 1, start_col + 1);

    int index = start_col;
    while (index <= end_col) {
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

void editorOutputInit(void) {
    gEditor.render_buffer = ABUF_INIT;

    gEditor.screen_size_updated = true;
    gEditor.screen_rows = 0;
    gEditor.screen_cols = 0;

    surfaceInit(&gEditor.screen, 0, 0);
    surfaceInit(&gEditor.old_screen, 0, 0);
}

void editorOutputFree(void) {
    surfaceFree(&gEditor.screen);
    surfaceFree(&gEditor.old_screen);
    abufFree(&gEditor.render_buffer);
}

static void editorDrawConMsg(Surface surface) {
    if (gEditor.con_size == 0) {
        return;
    }

    ScreenStyle style = {
        .fg = gEditor.color_cfg[UI_COLOR_PROMPT_FG],
        .bg = gEditor.color_cfg[UI_COLOR_PROMPT_BG],
    };

    // con_size + status bar
    int draw_row = surface.h - (gEditor.con_size + 1);
    if (panelIsEnabled((Panel*)gEditor.prompt_panel)) {
        draw_row--;  // TODO: Adjust for prompt panel height if needed
    }

    int index = gEditor.con_front;
    for (int i = 0; i < gEditor.con_size; i++) {
        ScreenCell* row = SURFACE_ROW(surface, draw_row);
        screenClearCells(row, surface.w, 0, surface.w, style);

        const char* buf = gEditor.con_msg[index];
        index = (index + 1) % EDITOR_CON_COUNT;

        screenPutUtf8(row, surface.w, 0, buf, style);

        draw_row++;
    }
}

static void editorDrawStatusBar(Surface surface) {
    if (surface.h != 1 || surface.w <= 0) {
        return;
    }

    int w = surface.w;
    ScreenCell* row = surface.cells;
    ScreenStyle default_style = {
        .fg = gEditor.color_cfg[UI_COLOR_STATUS_FG],
        .bg = gEditor.color_cfg[UI_COLOR_STATUS_BG],
    };

    screenClearCells(row, w, 0, w, default_style);

    EditorHelpMsg help_msg = helpinfo.int_value ? gEditor.help_msg : HELP_NONE;
    const char* help_str = editorHelpMsgToString(help_msg);

    char lang[16];
    char pos[64];
    int rlen = 0;

    EditorTab* tab = editorGetActiveTab();
    const EditorFile* file = editorTabGetFile(tab);
    if (file) {
        const char* file_type =
            file->syntax ? file->syntax->file_type : "Plain Text";
        int row_num = tab->cursor.y + 1;
        int col = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x) + 1;
        float line_percent = 0.0f;
        const char* nl_type = (file->newline == NL_UNIX) ? "LF" : "CRLF";
        if (file->num_rows - 1 > 0) {
            line_percent =
                (float)tab->row_offset / (file->num_rows - 1) * 100.0f;
        }

        snprintf(lang, sizeof(lang), "  %s  ", file_type);
        snprintf(pos, sizeof(pos), " %d:%d [%.f%%] <%s> ", row_num, col,
                 line_percent, nl_type);
        rlen = strUTF8Width(lang) + strUTF8Width(pos);
    }

    if (rlen > w)
        rlen = 0;

    int x = 0;
    ScreenStyle style = default_style;

    int max_help_width = (rlen > 0) ? w - rlen : w;
    if (max_help_width > 0) {
        x += screenPutAscii(row, max_help_width, 0, help_str, style);
    }

    if (rlen > 0 && surface.w - x >= rlen) {
        int right_x = surface.w - rlen;
        style.fg = gEditor.color_cfg[UI_COLOR_STATUS_LANG_FG];
        style.bg = gEditor.color_cfg[UI_COLOR_STATUS_LANG_BG];
        int lang_width = strUTF8Width(lang);
        screenPutAscii(row, surface.w, right_x, lang, style);
        style.fg = gEditor.color_cfg[UI_COLOR_STATUS_POS_FG];
        style.bg = gEditor.color_cfg[UI_COLOR_STATUS_POS_BG];
        screenPutAscii(row, surface.w, right_x + lang_width, pos, style);
    }
}

void editorRefreshScreen(void) {
    if (gEditor.screen_rows <= 0 || gEditor.screen_cols <= 0) {
        return;
    }

    if (gEditor.screen_size_updated) {
        surfaceFree(&gEditor.screen);
        surfaceFree(&gEditor.old_screen);

        surfaceInit(&gEditor.screen, gEditor.screen_cols, gEditor.screen_rows);
        surfaceInit(&gEditor.old_screen, gEditor.screen_cols,
                    gEditor.screen_rows);
    }

    abuf* ab = &gEditor.render_buffer;
    abufReset(ab);

    abufAppendStr(ab, ANSI_SYNC_BEGIN ANSI_CURSOR_HIDE);

    // One row for status bar
    Rect ui_rect = {0, 0, gEditor.screen_cols, gEditor.screen_rows - 1};
    Rect status_rect = {0, gEditor.screen_rows - 1, gEditor.screen_cols, 1};

    // TODO: Make a separate color for separators
    ScreenStyle sep_style = {
        .bg = gEditor.color_cfg[UI_COLOR_BG],
        .fg = gEditor.color_cfg[UI_COLOR_TOP_TABS_FG],
    };
    uiComposite(&gEditor.ui, surfaceSub(gEditor.screen, ui_rect), sep_style);

    // TODO: Refactor these into the new UI API
    editorDrawStatusBar(surfaceSub(gEditor.screen, status_rect));
    editorDrawConMsg(gEditor.screen);

    // Render sreen
    for (int i = 0; i < gEditor.screen_rows; i++) {
        int start_col = 0;
        int end_col = gEditor.screen_cols - 1;
        if (gEditor.screen_size_updated ||
            editorScreenRowUpdated(i, &start_col, &end_col)) {
            editorRenderRow(ab, i, start_col, end_col);
            // Save current screen
            memcpy(&SURFACE_AT(gEditor.old_screen, 0, i),
                   &SURFACE_AT(gEditor.screen, 0, i),
                   sizeof(ScreenCell) * gEditor.screen_cols);
        }
    }

    if (gEditor.screen_size_updated) {
        gEditor.screen_size_updated = false;
    }

    // Crosshair
    UICursor cursor;
    if (uiGetCursor(&gEditor.ui, &cursor)) {
        // Make sure the cursor is within the screen
        if (cursor.x >= 0 && cursor.x < gEditor.screen_cols && cursor.y >= 0 &&
            cursor.y < gEditor.screen_rows) {
            gotoXY(ab, cursor.y + 1, cursor.x + 1);
            abufAppendStr(ab, ANSI_CURSOR_SHOW);
        }
    }

    abufAppendStr(ab, ANSI_CLEAR_STYLE ANSI_SYNC_END);

    writeConsoleAll(ab->buf, ab->len);
}
