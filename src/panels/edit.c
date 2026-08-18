#include "panels/edit.h"

#include "config.h"
#include "console.h"
#include "search.h"
#include "unicode.h"

#include "panels/explorer.h"

#define EDITOR_SCROLL_DIST 3

static void destroy(Panel* self);
static void render(Panel* self, Surface s);
static bool getCursor(Panel* self, UICursor* out);
static void onFocus(Panel* self, bool focused);
static void keyEvent(Panel* self, EditorInput input);
static bool mouseEvent(Panel* self, UIMouseEvent mouse_event);

static PanelVtable panel_vt = {
    .destroy = destroy,
    .render = render,
    .getCursor = getCursor,
    .onFocus = onFocus,
    .keyEvent = keyEvent,
    .mouseEvent = mouseEvent,
};

EditPanel* panelEditCreate(void) {
    EditPanel* p = calloc_s(1, sizeof(EditPanel));
    p->base.vt = &panel_vt;
    p->base.kind = PANEL_KIND_EDIT;

    p->wait_state = EDIT_WAIT_NONE;
    p->wait_tab_index = -1;
    p->tab_active_index = -1;
    return p;
}

static void destroy(Panel* self) {
    EditPanel* p = (EditPanel*)self;
    for (uint32_t i = 0; i < p->tabs.size; i++) {
        editorRemoveFile(p->tabs.data[i].file_index);
    }
    vector_free(p->tabs);

    if (gEditor.active_edit_panel == p) {
        gEditor.active_edit_panel = NULL;
    }
}

static void getTabName(const EditorTab* tab, char* out_name, size_t out_size) {
    if (!out_name || out_size == 0) {
        return;
    }

    if (!tab) {
        out_name[0] = '\0';
        return;
    }

    const EditorFile* file = editorTabGetFileConst(tab);
    if (!file) {
        out_name[0] = '\0';
        return;
    }

    if (file->filename) {
        const char* basename = getBaseName(file->filename);
        snprintf(out_name, out_size, " %s%s%s ", file->read_only ? "[RO]" : "",
                 file->dirty ? "*" : "", basename);
    } else {
        snprintf(out_name, out_size, " %s%sUntitled-%d ",
                 file->read_only ? "[RO]" : "", file->dirty ? "*" : "",
                 file->new_id + 1);
    }
}

typedef struct TabDisplayResult {
    int tab_displayed;
    int left_indicator_x;   // -1 if not displayed
    int right_indicator_x;  // -1 if not displayed
} TabDisplayResult;

typedef void (*TabDisplayCallback)(int index,
                                   int x,
                                   int display_width,
                                   const char* display_name,
                                   bool active_tab,
                                   void* user_data);

static TabDisplayResult iterateDisplayTabs(const EditPanel* split,
                                           int w,
                                           TabDisplayCallback callback,
                                           void* user_data) {
    TabDisplayResult result = {
        .tab_displayed = 0,
        .left_indicator_x = -1,
        .right_indicator_x = -1,
    };
    if (!split || w == 0)
        return result;

    int x = 0;
    int i = split->tab_offset;
    if (i != 0) {
        result.left_indicator_x = x;
        x++;  // For the "<" indicator
    }

    while (x < w && i < (int)split->tabs.size) {
        const EditorTab* tab = &split->tabs.data[i];

        bool active_tab = (i == split->tab_active_index);

        char buf[EDITOR_PATH_MAX] = {0};
        getTabName(tab, buf, sizeof(buf));
        int tab_width = strUTF8Width(buf);

        bool is_last_tab = (i == (int)split->tabs.size - 1);
        if (is_last_tab && x + tab_width > w) {
            if (i == split->tab_offset) {
                // Display at least one tab (truncated)
                result.tab_displayed++;
                if (callback) {
                    callback(i, x, w - x, buf, active_tab, user_data);
                }
            } else {
                result.right_indicator_x = x;
            }
            break;
        } else if (!is_last_tab &&
                   x + tab_width > w - 1) {  // -1 for the ">" indicator
            if (i == split->tab_offset) {
                // Display at least one tab (truncated)
                result.tab_displayed++;
                if (callback) {
                    tab_width =
                        w - x - (is_last_tab ? 0 : 1);  // For the ">" indicator
                    callback(i, x, tab_width, buf, active_tab, user_data);
                    x += tab_width;
                }
                if (!is_last_tab) {
                    result.right_indicator_x = x;
                }
            } else {
                // Not the last tab
                result.right_indicator_x = x;
            }
            break;
        }

        result.tab_displayed++;
        if (callback) {
            callback(i, x, tab_width, buf, active_tab, user_data);
        }
        x += tab_width;
        i++;
    }

    return result;
}

typedef struct TabBarDrawData {
    ScreenCell* row;
    ScreenStyle tab_default_style;
    ScreenStyle tab_active_style;
    int max_width;
} TabBarDrawData;

static void drawTabCallback(int index,
                            int x,
                            int display_width,
                            const char* display_name,
                            bool active_tab,
                            void* user_data) {
    UNUSED(index);
    UNUSED(display_width);

    TabBarDrawData* data = (TabBarDrawData*)user_data;
    ScreenStyle style =
        active_tab ? data->tab_active_style : data->tab_default_style;
    screenPutUtf8(data->row, data->max_width, x, display_name, style);
}

static void drawTabBar(const EditPanel* split, ScreenCell* row, int w) {
    if (!row || w <= 0) {
        return;
    }

    bool focused = (gEditor.active_edit_panel == split);

    const ScreenStyle default_style = {
        .fg = gEditor.color_cfg[UI_COLOR_TOP_FG],
        .bg = gEditor.color_cfg[UI_COLOR_TOP_BG],
    };
    const ScreenStyle tab_default_style = {
        .fg = gEditor.color_cfg[UI_COLOR_TOP_TABS_FG],
        .bg = gEditor.color_cfg[UI_COLOR_TOP_TABS_BG],
    };

    // Active split active tab style
    const ScreenStyle tab_active_style = focused ? (ScreenStyle){
        .fg = gEditor.color_cfg[UI_COLOR_TOP_SELECT_FG],
        .bg = gEditor.color_cfg[UI_COLOR_TOP_SELECT_BG],
    } : (ScreenStyle){
        .fg = gEditor.color_cfg[UI_COLOR_TOP_SELECT_FG],
        .bg = gEditor.color_cfg[UI_COLOR_TOP_TABS_BG],
    };

    screenClearCells(row, w, 0, w, default_style);

    TabBarDrawData draw_data = {
        .row = row,
        .tab_default_style = tab_default_style,
        .tab_active_style = tab_active_style,
        .max_width = w,
    };
    TabDisplayResult result =
        iterateDisplayTabs(split, w, drawTabCallback, &draw_data);

    if (result.left_indicator_x != -1) {
        screenPutAscii(row, w, result.left_indicator_x, "<", default_style);
    }
    if (result.right_indicator_x != -1) {
        screenPutAscii(row, w, result.right_indicator_x, ">", default_style);
    }
}

static void drawContent(EditPanel* split, Surface s) {
    if (!split || s.w <= 0 || s.h <= 0) {
        return;
    }

    EditorTab* tab = editorSplitGetTab(split);
    EditorFile* file = editorTabGetFile(tab);

    EditorSelectRange range = {0};
    if (tab->cursor.is_selected)
        editorGetSelectRange(&tab->cursor, &range);

    for (int i = tab->row_offset, y = 0; i < tab->row_offset + s.h; i++, y++) {
        ScreenCell* row = SURFACE_ROW(s, y);

        // Clear the entire row
        EditorUIColorType bg_color =
            (i == tab->cursor.y && !tab->cursor.is_selected)
                ? UI_COLOR_CURSORLINE
                : UI_COLOR_BG;
        screenClearCells(row, s.w, 0, s.w,
                         (ScreenStyle){
                             .fg = gEditor.color_cfg[UI_COLOR_HL_NORMAL],
                             .bg = gEditor.color_cfg[bg_color],
                         });

        if (i >= file->num_rows)
            continue;

        int x = 0;

        // Draw line number
        if (lineno.int_value) {
            ScreenStyle lineno_style;
            lineno_style.fg = (i == tab->cursor.y)
                                  ? gEditor.color_cfg[UI_COLOR_LINENO_BG]
                                  : gEditor.color_cfg[UI_COLOR_LINENO_FG];
            lineno_style.bg = (i == tab->cursor.y)
                                  ? gEditor.color_cfg[UI_COLOR_LINENO_FG]
                                  : gEditor.color_cfg[UI_COLOR_LINENO_BG];

            char line_number[16];
            snprintf(line_number, sizeof(line_number), " %*d ",
                     file->lineno_width - 2, i + 1);
            x += screenPutAscii(row, s.w, x, line_number, lineno_style);
        }

        // Lazy syntax update
        EditorRow* row_data = &file->row[i];
        if (!row_data->hl_updated) {
            // Do full single line syntax update
            editorUpdateSyntax(file, row_data, HL_UPDATE_SINGLE_LINE);
        }

        // Draw content
        uint32_t hls_index = 0;
        const EditorHLSpanVector hl_spans = row_data->hl_spans;

        uint32_t rx = tab->col_offset;
        uint32_t cx = editorRowRxToCx(row_data, rx);

        while (x < s.w && (int)cx < row_data->size) {
            EditorUIColorType fg = UI_COLOR_HL_NORMAL;
            EditorUIColorType bg = bg_color;

            // We assume the span is sorted and not overlapping
            while (hls_index < hl_spans.size &&
                   hl_spans.data[hls_index].start +
                           hl_spans.data[hls_index].len <=
                       cx) {
                hls_index++;
            }

            if (hls_index < hl_spans.size) {
                const EditorHLSpan* span = &hl_spans.data[hls_index];
                uint32_t span_start = span->start;
                uint32_t span_end = span->start + span->len;
                if (span_start <= cx && cx < span_end) {
                    EditorUIColorType hl_color = editorHL2UIColor(span->type);
                    fg = hl_color;
                }
            }

            if (tab->cursor.is_selected && editorIsPosSelected(i, cx, range)) {
                bg = UI_COLOR_HL_SELECT;
            } else if (tab->has_match && i == tab->match_row &&
                       cx >= tab->match_col &&
                       cx < tab->match_col + tab->match_len) {
                bg = UI_COLOR_HL_MATCH;
            } else if (row_data->size - row_data->trailing_spaces <= cx) {
                bg = UI_COLOR_HL_TRAILING;
            }

            ScreenStyle style = {0};
            const char* c = &row_data->data[cx];

            if (isCntrl(*c) && *c != '\t') {
                // Control character (show inverted)
                style.fg = gEditor.color_cfg[fg];
                style.bg = gEditor.color_cfg[bg];

                uint32_t sym = (*c <= 26) ? '@' + *c : '?';
                Color tmp = style.fg;
                style.fg = style.bg;
                style.bg = tmp;

                x += screenPutChar(row, s.w, x, sym, &style);
                rx++;
                cx++;
            } else {
                if (drawspace.int_value && (*c == ' ' || *c == '\t')) {
                    fg = UI_COLOR_HL_SPACE;
                }
                if (bg == UI_COLOR_HL_TRAILING && !trailing.int_value) {
                    bg = bg_color;
                }

                style.fg = gEditor.color_cfg[fg];
                style.bg = gEditor.color_cfg[bg];

                if (*c == '\t') {
                    char tab_char = drawspace.int_value ? '|' : ' ';
                    x += screenPutChar(row, s.w, x, tab_char, &style);
                    rx++;
                    while (rx % tabsize.int_value != 0 && x < s.w) {
                        x += screenPutChar(row, s.w, x, ' ', &style);
                        rx++;
                    }
                    cx++;
                } else if (*c == ' ') {
                    char space_char = drawspace.int_value ? '.' : ' ';
                    x += screenPutChar(row, s.w, x, space_char, &style);
                    rx++;
                    cx++;
                } else {
                    Grapheme grapheme = {0};
                    size_t byte_size;
                    uint32_t unicode =
                        decodeUTF8(c, row_data->size - cx, &byte_size);
                    cx += byte_size;

                    int width = unicodeWidth(unicode);
                    if (width < 0) {
                        grapheme.cluster[0] = 0xFFFD;
                        grapheme.size = 1;
                        grapheme.width = 1;
                    } else if (width > 0) {
                        grapheme.cluster[0] = unicode;
                        grapheme.size = 1;
                        grapheme.width = width;
                        // Ganging zero-width characters to the grapheme cluster
                        // This is not perfect, but it works for most cases.
                        while ((int)cx < row_data->size) {
                            size_t next_byte_size;
                            uint32_t next_unicode = decodeUTF8(
                                &row_data->data[cx], row_data->size - cx,
                                &next_byte_size);
                            int next_width = unicodeWidth(next_unicode);
                            if (next_width == 0 &&
                                grapheme.size < MAX_CLUSTER_SIZE) {
                                grapheme.cluster[grapheme.size] = next_unicode;
                                grapheme.size++;
                                cx += next_byte_size;
                            } else {
                                break;
                            }
                        }
                    }

                    x += screenPutGrapheme(row, s.w, x, &grapheme, &style);
                    rx += grapheme.width;
                }
            }
        }

        // Add newline character when selected
        if (tab->cursor.is_selected && range.end_y > i && i >= range.start_y &&
            x < s.w) {
            ScreenStyle select_style = {
                .fg = gEditor.color_cfg[UI_COLOR_HL_NORMAL],
                .bg = gEditor.color_cfg[UI_COLOR_HL_SELECT],
            };
            screenPutChar(row, s.w, x, ' ', &select_style);
        }
    }
}

static void render(Panel* self, Surface s) {
    EditPanel* split = (EditPanel*)self;

    if (s.h <= 0 || s.w <= 0)
        return;

    drawTabBar(split, SURFACE_ROW(s, 0), s.w);
    drawContent(split, surfaceSub(s, (Rect){0, 1, s.w, s.h - 1}));
}

static bool getCursor(Panel* self, UICursor* out) {
    EditPanel* p = (EditPanel*)self;

    bool visible = false;
    int out_x = 0;
    int out_y = 0;

    const EditorTab* tab = editorSplitGetTabConst(p);
    const EditorFile* file = editorTabGetFileConst(tab);

    if (file) {
        const int tab_bar_height = 1;
        const int lineno_width = lineno.int_value ? file->lineno_width : 0;
        const int content_width = p->base.layout->rect.w - lineno_width;
        const int content_height = p->base.layout->rect.h - tab_bar_height;

        if (tab->cursor.y >= tab->row_offset &&
            tab->cursor.y < tab->row_offset + content_height &&
            tab->cursor.x >= tab->col_offset &&
            tab->cursor.x < tab->col_offset + content_width) {
            visible = true;
            out_y = tab->cursor.y - tab->row_offset + tab_bar_height;

            int rx = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
            out_x = rx - tab->col_offset + lineno_width;
        }
    }

    if (out) {
        out->visible = visible;
        out->x = out_x;
        out->y = out_y;
    }

    return visible;
}

static void onFocus(Panel* self, bool focused) {
    EditPanel* p = (EditPanel*)self;

    editorCancelPendingWait(p);

    if (focused) {
        gEditor.active_edit_panel = p;

        EditorTab* tab = editorSplitGetTab(p);
        if (tab) {
            tab->bracket_autocomplete = 0;
        }

        editorHelpSetMsg(HELP_EDIT);
    }
}

static void editorMousePosToEditorPos(const EditPanel* split,
                                      int local_x,
                                      int local_y,
                                      int* out_x,
                                      int* out_y) {
    if (!out_x || !out_y)
        return;
    *out_x = 0;
    *out_y = 0;

    if (!split)
        return;

    const int tab_bar_height = 1;

    Rect rect = split->base.layout->rect;
    if (local_x < 0)
        local_x = 0;
    if (local_x >= rect.w)
        local_x = rect.w - 1;

    if (local_y < tab_bar_height)
        local_y = tab_bar_height;
    if (local_y >= rect.h)
        local_y = rect.h - 1;

    const EditorTab* tab = editorSplitGetTabConst(split);
    const EditorFile* file = editorTabGetFileConst(tab);

    int row = tab->row_offset + local_y - tab_bar_height;
    if (row < 0)
        return;

    if (row >= file->num_rows) {
        *out_y = file->num_rows - 1;
        *out_x = file->row[*out_y].rsize;
        return;
    }

    const int lineno_width = lineno.int_value ? file->lineno_width : 0;
    int col = local_x - lineno_width + tab->col_offset;
    if (col < 0) {
        col = 0;
    } else if (col > file->row[row].rsize) {
        col = file->row[row].rsize;
    }

    *out_x = col;
    *out_y = row;
}

static void editorMoveCursor(EditorTab* tab, int e) {
    const EditorFile* file = editorTabGetFile(tab);
    const EditorRow* row = &file->row[tab->cursor.y];

    switch (e) {
        case ARROW_LEFT:
            if (tab->cursor.x != 0) {
                tab->cursor.x = editorRowPreviousUTF8(&file->row[tab->cursor.y],
                                                      tab->cursor.x);
            } else if (tab->cursor.y > 0) {
                tab->cursor.y--;
                tab->cursor.x = file->row[tab->cursor.y].size;
            }
            editorUpdateSx(tab);
            break;

        case ARROW_RIGHT:
            if (row && tab->cursor.x < row->size) {
                tab->cursor.x =
                    editorRowNextUTF8(&file->row[tab->cursor.y], tab->cursor.x);
                editorUpdateSx(tab);
            } else if (row && (tab->cursor.y + 1 < file->num_rows) &&
                       tab->cursor.x == row->size) {
                tab->cursor.y++;
                tab->cursor.x = 0;
                tab->sx = 0;
            }
            break;

        case ARROW_UP:
            if (tab->cursor.y != 0) {
                tab->cursor.y--;
                tab->cursor.x =
                    editorRowRxToCx(&file->row[tab->cursor.y], tab->sx);
            }
            break;

        case ARROW_DOWN:
            if (tab->cursor.y + 1 < file->num_rows) {
                tab->cursor.y++;
                tab->cursor.x =
                    editorRowRxToCx(&file->row[tab->cursor.y], tab->sx);
            }
            break;
    }
    row = (tab->cursor.y >= file->num_rows) ? NULL : &file->row[tab->cursor.y];
    int row_len = row ? row->size : 0;
    if (tab->cursor.x > row_len) {
        tab->cursor.x = row_len;
    }
}

static void editorMoveCursorWordLeft(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);

    if (tab->cursor.x == 0) {
        if (tab->cursor.y == 0)
            return;
        editorMoveCursor(tab, ARROW_LEFT);
    }

    const EditorRow* row = &file->row[tab->cursor.y];
    tab->cursor.x = editorRowWordLeft(row, tab->cursor.x);
    editorUpdateSx(tab);
}

static void editorMoveCursorWordRight(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);

    if (tab->cursor.x == file->row[tab->cursor.y].size) {
        if (tab->cursor.y == file->num_rows - 1)
            return;
        tab->cursor.x = 0;
        tab->cursor.y++;
    }

    const EditorRow* row = &file->row[tab->cursor.y];
    tab->cursor.x = editorRowWordRight(row, tab->cursor.x);
    editorUpdateSx(tab);
}

static void editorSelectWord(EditorTab* tab,
                             const EditorRow* row,
                             int cx,
                             IsCharFunc is_char) {
    int select_start, select_end;
    editorRowSelectWord(row, cx, is_char, &select_start, &select_end);
    tab->cursor.select_x = select_start;
    tab->cursor.x = select_end;
    tab->cursor.select_y = tab->cursor.y;
    tab->cursor.is_selected = true;
    editorUpdateSx(tab);
}

static void editorSelectLine(EditorTab* tab, int row) {
    const EditorFile* file = editorTabGetFile(tab);

    if (row < 0 || row >= file->num_rows)
        return;

    tab->cursor.is_selected = true;
    tab->cursor.select_x = 0;
    tab->cursor.select_y = row;
    tab->bracket_autocomplete = 0;

    if (row < file->num_rows - 1) {
        tab->cursor.y = row + 1;
        tab->cursor.x = 0;
    } else {
        tab->cursor.y = row;
        tab->cursor.x = file->row[row].size;
        if (tab->cursor.x == 0) {
            tab->cursor.is_selected = false;
        }
    }

    editorUpdateSx(tab);
}

static void editorSelectAll(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);

    if (file->num_rows == 1 && file->row[0].size == 0)
        return;
    tab->cursor.is_selected = true;
    tab->bracket_autocomplete = 0;
    tab->cursor.y = file->num_rows - 1;
    tab->cursor.x = file->row[file->num_rows - 1].size;
    editorUpdateSx(tab);
    tab->cursor.select_y = 0;
    tab->cursor.select_x = 0;
}

static void keyEvent(Panel* self, EditorInput input) {
    EditPanel* p = (EditPanel*)self;

    EditWaitState wait_state = p->wait_state;
    p->wait_state = EDIT_WAIT_NONE;

    EditorTab* tab = editorSplitGetTab(p);
    EditorFile* file = editorTabGetFile(tab);

    bool should_scroll = false;
    bool keep_selection = false;
    bool keep_bracket_autocomplete = false;

    // Edit action
    bool has_edit = false;
    Edit edit = {0};
    bool should_set_edit_cursor = false;
    EditorCursor old_cursor = tab->cursor;
    EditorCursor new_cursor = tab->cursor;

    int e = input.type;
    switch (e) {
        // --- File ---

        // Save
        case CTRL_KEY('s'): {
            keep_bracket_autocomplete = true;
            keep_selection = true;

            if (wait_state == EDIT_WAIT_SAVE) {
                // Handle save confirmation
                if (!file->filename) {
                    editorPromptSaveAs(file);
                } else {
                    editorSave(file, file->filename);
                }
                break;
            }

            if (!file->dirty && file->filename) {
                // Nothing to save
                break;
            }

            editorMsgClear();

            bool warn = editorIsDangerousSave(file, true);
            if (!warn && file->read_only && file->unlocked) {
                // File was read-only at open but permissions may have since
                // changed
                editorMsg("File was read-only when opened.");
                warn = true;
            }
            if (warn) {
                editorMsg("Press save again to save anyway.");
                p->wait_state = EDIT_WAIT_SAVE;
                gEditor.pending_edit_panel = p;
                break;
            }

            if (!file->filename) {
                editorPromptSaveAs(file);
            } else {
                editorSave(file, file->filename);
            }
            break;
        }

        // Save all
        case ALT_KEY('s'): {
            keep_bracket_autocomplete = true;
            keep_selection = true;

            bool has_readonly = false;
            bool has_dangerous = false;
            bool has_untitled = false;
            for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
                EditorFile* f = &gEditor.files[i];
                if (f->reference_count > 0 && (f->dirty || !f->filename)) {
                    if (!f->filename) {
                        has_untitled = true;
                        continue;
                    }
                    if (f->read_only) {
                        has_readonly = true;
                        continue;
                    }
                    if (editorIsDangerousSave(f, false)) {
                        has_dangerous = true;
                        continue;
                    }

                    editorSave(f, f->filename);
                }
            }

            editorMsgClear();

            if (has_untitled) {
                // TODO: Show prompts to save untitled files
                editorMsg("Some files were skipped (untitled).");
            }
            if (has_dangerous) {
                editorMsg(
                    "Some files were skipped (modified or permissions "
                    "changed).");
            }
            if (has_readonly) {
                editorMsg("Some read-only files were skipped.");
            }
            if (has_untitled || has_dangerous || has_readonly) {
                editorMsg("Please save them individually.");
            }
        } break;

        // Save as
        case ALT_KEY('a'):
            keep_bracket_autocomplete = true;
            keep_selection = true;

            editorPromptSaveAs(file);
            break;

        // Next file
        case CTRL_KEY(']'):
            keep_bracket_autocomplete = true;
            keep_selection = true;

            if (p->tabs.size < 2)
                break;
            if (p->tab_active_index == (int)p->tabs.size - 1) {
                editorChangeToFile(p, 0);
            } else {
                editorChangeToFile(p, p->tab_active_index + 1);
            }

            tab = NULL;
            file = NULL;
            break;

        // Split left right
        case CTRL_KEY('\\'): {
            EditPanel* new_split = editorAddSplit(p, true);
            editorAddTab(new_split, editorSplitGetTab(p)->file_index);
            uiPanelSetFocused(&gEditor.ui, (Panel*)new_split);
        } break;

        // Split top bottom
        case CTRL_KEY('_'): {
            EditPanel* new_split = editorAddSplit(p, false);
            editorAddTab(new_split, editorSplitGetTab(p)->file_index);
            uiPanelSetFocused(&gEditor.ui, (Panel*)new_split);
        } break;

        // --- Navigation & Selection ---

        // Unselect
        case ESC:
            if (tab->cursor.is_selected) {
                should_scroll = true;
            }
            break;

        // Move cursor
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            should_scroll = true;
            // Will reset this manually if needed
            keep_bracket_autocomplete = true;

            if (tab->cursor.is_selected) {
                EditorSelectRange range;
                editorGetSelectRange(&tab->cursor, &range);

                if (e == ARROW_UP || e == ARROW_LEFT) {
                    tab->cursor.x = range.start_x;
                    tab->cursor.y = range.start_y;
                } else {
                    tab->cursor.x = range.end_x;
                    tab->cursor.y = range.end_y;
                }
                editorUpdateSx(tab);
                if (e == ARROW_UP || e == ARROW_DOWN) {
                    editorMoveCursor(tab, e);
                }
                tab->cursor.is_selected = false;
            } else {
                if (tab->bracket_autocomplete) {
                    if (e == ARROW_RIGHT) {
                        tab->bracket_autocomplete--;
                    } else {
                        tab->bracket_autocomplete = 0;
                    }
                }
                editorMoveCursor(tab, e);
            }
            break;

        case SHIFT_UP:
        case SHIFT_DOWN:
        case SHIFT_LEFT:
        case SHIFT_RIGHT:
            should_scroll = true;
            keep_selection = true;

            tab->cursor.is_selected = true;
            editorMoveCursor(tab, e - (SHIFT_UP - ARROW_UP));
            break;

        // Word move
        case CTRL_LEFT:
        case SHIFT_CTRL_LEFT:
            should_scroll = true;
            keep_selection = true;

            editorMoveCursorWordLeft(tab);
            tab->cursor.is_selected = (e == SHIFT_CTRL_LEFT);
            break;

        case CTRL_RIGHT:
        case SHIFT_CTRL_RIGHT:
            should_scroll = true;
            keep_selection = true;

            editorMoveCursorWordRight(tab);
            tab->cursor.is_selected = (e == SHIFT_CTRL_RIGHT);
            break;

        // Select word
        case CTRL_KEY('d'): {
            should_scroll = true;
            keep_selection = true;

            const EditorRow* row = &file->row[tab->cursor.y];
            if (tab->cursor.x < row->size &&
                !isIdentifierChar(row->data[tab->cursor.x])) {
                should_scroll = false;
                break;
            }
            editorSelectWord(tab, row, tab->cursor.x, isNonIdentifierChar);
        } break;

        // Move to start/end of line
        case HOME_KEY:
        case SHIFT_HOME: {
            should_scroll = true;
            keep_selection = true;

            int start_x = editorRowNextCharIndex(&file->row[tab->cursor.y], 0,
                                                 isNonSpace);
            if (start_x == tab->cursor.x)
                start_x = 0;
            tab->cursor.x = start_x;
            editorUpdateSx(tab);
            tab->cursor.is_selected = (e == (SHIFT_HOME));
        } break;

        case END_KEY:
        case SHIFT_END:
            should_scroll = true;
            keep_selection = true;

            tab->cursor.x = file->row[tab->cursor.y].size;
            editorUpdateSx(tab);
            tab->cursor.is_selected = (e == SHIFT_END);
            break;

        // Move to start/end of file
        case CTRL_HOME:
        case SHIFT_CTRL_HOME:
            should_scroll = true;
            keep_selection = true;

            tab->cursor.is_selected = (e == SHIFT_CTRL_HOME);
            tab->cursor.y = 0;
            tab->cursor.x = 0;
            tab->sx = 0;
            editorUpdateSx(tab);
            break;

        case CTRL_END:
        case SHIFT_CTRL_END:
            should_scroll = true;
            keep_selection = true;

            tab->cursor.is_selected = (e == SHIFT_CTRL_END);
            tab->cursor.y = file->num_rows - 1;
            tab->cursor.x = file->row[file->num_rows - 1].size;
            editorUpdateSx(tab);
            break;

        // Next/previous page
        case SHIFT_PAGE_UP:
        case SHIFT_PAGE_DOWN:
        case PAGE_UP:
        case PAGE_DOWN: {
            should_scroll = true;
            keep_selection = true;

            const int tab_bar_height = 1;
            const int content_height = p->base.layout->rect.h - tab_bar_height;

            tab->cursor.is_selected =
                (e == SHIFT_PAGE_UP || e == SHIFT_PAGE_DOWN);

            if (e == PAGE_UP || e == SHIFT_PAGE_UP) {
                tab->cursor.y = tab->row_offset;
            } else if (e == PAGE_DOWN || e == SHIFT_PAGE_DOWN) {
                tab->cursor.y = tab->row_offset + content_height - 1;
                if (tab->cursor.y >= file->num_rows)
                    tab->cursor.y = file->num_rows - 1;
            }

            int times = content_height;
            while (times--) {
                if (e == PAGE_UP || e == SHIFT_PAGE_UP) {
                    if (tab->cursor.y == 0) {
                        tab->cursor.x = 0;
                        tab->sx = 0;
                        editorUpdateSx(tab);
                        break;
                    }
                    editorMoveCursor(tab, ARROW_UP);
                } else {
                    if (tab->cursor.y == file->num_rows - 1) {
                        tab->cursor.x = file->row[tab->cursor.y].size;
                        editorUpdateSx(tab);
                        break;
                    }
                    editorMoveCursor(tab, ARROW_DOWN);
                }
            }
        } break;

        // Next/previous empty line
        case SHIFT_CTRL_PAGE_UP:
        case CTRL_PAGE_UP:
            should_scroll = true;
            keep_selection = true;

            tab->cursor.is_selected = (e == SHIFT_CTRL_PAGE_UP);
            while (tab->cursor.y > 0) {
                editorMoveCursor(tab, ARROW_UP);
                if (file->row[tab->cursor.y].size == 0) {
                    break;
                }
            }
            break;

        case SHIFT_CTRL_PAGE_DOWN:
        case CTRL_PAGE_DOWN:
            should_scroll = true;
            keep_selection = true;

            tab->cursor.is_selected = (e == SHIFT_CTRL_PAGE_DOWN);
            while (tab->cursor.y < file->num_rows - 1) {
                editorMoveCursor(tab, ARROW_DOWN);
                if (file->row[tab->cursor.y].size == 0) {
                    break;
                }
            }
            break;

        // Select line
        case CTRL_KEY('l'):
            should_scroll = true;
            keep_selection = true;

            editorSelectLine(tab, tab->cursor.y);
            break;

        // Select all
        case CTRL_KEY('a'):
            keep_selection = true;

            editorSelectAll(tab);
            break;

        // Find
        case CTRL_KEY('f'):
            editorPromptFind();
            break;

        // Goto line
        case CTRL_KEY('g'):
            editorPromptGoto();
            break;

        // Scroll
        case CTRL_UP:
        case CTRL_DOWN: {
            keep_selection = true;
            keep_bracket_autocomplete = true;
            editorScroll(p, e == CTRL_UP ? -1 : 1);
        } break;

        // --- Edit ---

        // Copy
        case CTRL_KEY('c'): {
            keep_selection = true;

            editorFreeClipboardContent(&gEditor.clipboard);

            if (tab->cursor.is_selected) {
                EditorSelectRange range;
                editorGetSelectRange(&tab->cursor, &range);
                gEditor.copy_line = false;
                editorCopyText(file, &gEditor.clipboard, range);
            } else {
                editorCopyLine(file, &gEditor.clipboard, tab->cursor.y);
                gEditor.copy_line = true;
            }
            editorCopyToSysClipboard(&gEditor.clipboard, file->newline);
        } break;

        // Paste
        case PASTE_INPUT:
        case CTRL_KEY('v'): {
            bool is_paste_input = (e == PASTE_INPUT);
            EditorClipboard* clipboard =
                is_paste_input ? &input.data.paste : &gEditor.clipboard;

            if (!clipboard->size)
                break;

            should_scroll = true;
            has_edit = true;

            bool copy_line = is_paste_input ? false : gEditor.copy_line;
            EditorSelectRange delete_range = {
                tab->cursor.x,
                tab->cursor.y,
                tab->cursor.x,
                tab->cursor.y,
            };
            if (tab->cursor.is_selected) {
                editorGetSelectRange(&tab->cursor, &delete_range);
                editorCopyText(file, &edit.before, delete_range);
            }

            edit.x = delete_range.start_x;
            edit.y = delete_range.start_y;
            editorFreeClipboardContent(&edit.after);
            if (clipboard->size > 0) {
                edit.after.size = clipboard->size;
                edit.after.lines = malloc_s(sizeof(Str) * edit.after.size);
                for (size_t i = 0; i < clipboard->size; i++) {
                    edit.after.lines[i].size = clipboard->lines[i].size;
                    if (clipboard->lines[i].size == 0) {
                        edit.after.lines[i].data = NULL;
                    } else {
                        edit.after.lines[i].data =
                            malloc_s((size_t)clipboard->lines[i].size);
                        memcpy(edit.after.lines[i].data,
                               clipboard->lines[i].data,
                               (size_t)clipboard->lines[i].size);
                    }
                }
            }

            if (!tab->cursor.is_selected && copy_line) {
                edit.x = 0;
            }

            should_set_edit_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            if (edit.after.size == 0) {
                new_cursor.x = edit.x;
                new_cursor.y = edit.y;
            } else if (edit.after.size == 1) {
                new_cursor.x = edit.x + edit.after.lines[0].size;
                new_cursor.y = edit.y;
            } else {
                new_cursor.y = edit.y + (int)edit.after.size - 1;
                new_cursor.x = edit.after.lines[edit.after.size - 1].size;
            }
        } break;

        // Cut
        case CTRL_KEY('x'): {
            if (file->num_rows == 1 && file->row[0].size == 0)
                break;

            should_scroll = true;
            has_edit = true;

            editorFreeClipboardContent(&gEditor.clipboard);

            if (!tab->cursor.is_selected) {
                // Copy line
                editorCopyLine(file, &gEditor.clipboard, tab->cursor.y);
                gEditor.copy_line = true;

                // Delete line
                EditorSelectRange range = {0, tab->cursor.y,
                                           file->row[tab->cursor.y].size,
                                           tab->cursor.y};
                if (file->num_rows != 1) {
                    if (tab->cursor.y == file->num_rows - 1) {
                        range.start_y--;
                        range.start_x = file->row[range.start_y].size;
                    } else {
                        range.end_y++;
                        range.end_x = 0;
                    }
                }

                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(file, &edit.before, range);
                editorFreeClipboardContent(&edit.after);
                should_set_edit_cursor = true;
                new_cursor = tab->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
            } else {
                EditorSelectRange range;
                editorGetSelectRange(&tab->cursor, &range);
                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(file, &edit.before, range);
                editorFreeClipboardContent(&edit.after);
                editorCopyText(file, &gEditor.clipboard, range);
                gEditor.copy_line = false;
                should_set_edit_cursor = true;
                new_cursor = tab->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
            }
            editorCopyToSysClipboard(&gEditor.clipboard, file->newline);
        } break;

        // Undo
        case CTRL_KEY('z'): {
            keep_selection = true;
            keep_bracket_autocomplete = true;

            bool undo_applied = editorUndo(tab);
            should_scroll = undo_applied;
            if (undo_applied) {
                keep_bracket_autocomplete = false;
            }
        } break;

        // Redo
        case CTRL_KEY('y'): {
            keep_selection = true;
            keep_bracket_autocomplete = true;

            bool redo_applied = editorRedo(tab);
            should_scroll = redo_applied;
            if (redo_applied) {
                keep_bracket_autocomplete = false;
            }
        } break;

        // Duplicate line up/down
        case SHIFT_ALT_UP:
        case SHIFT_ALT_DOWN:
            should_scroll = true;
            has_edit = true;

            edit.x = 0;
            edit.y = tab->cursor.y;
            editorCopyLine(file, &edit.after, tab->cursor.y);
            should_set_edit_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            if (e == SHIFT_ALT_DOWN) {
                new_cursor.y++;
            }
            break;

        // Move line up/down
        case ALT_UP:
        case ALT_DOWN: {
            bool move_up = (e == ALT_UP);
            EditorSelectRange range;
            editorGetSelectRange(&tab->cursor, &range);
            if (move_up) {
                if (range.start_y == 0)
                    break;
            } else {
                if (range.end_y == file->num_rows - 1)
                    break;
            }

            should_scroll = true;
            keep_selection = true;
            has_edit = true;

            edit.x = 0;
            edit.y = move_up ? range.start_y - 1 : range.start_y;
            range.start_x = 0;
            if (move_up) {
                range.start_y--;
                range.end_x = file->row[range.end_y].size;
            } else {
                range.end_y++;
                range.end_x = file->row[range.end_y].size;
            }
            editorCopyText(file, &edit.before, range);
            editorFreeClipboardContent(&edit.after);
            edit.after.size = edit.before.size;
            edit.after.lines = malloc_s(sizeof(Str) * edit.after.size);
            for (size_t i = 0; i < edit.before.size; i++) {
                edit.after.lines[i].size = edit.before.lines[i].size;
                if (edit.before.lines[i].size == 0) {
                    edit.after.lines[i].data = NULL;
                } else {
                    edit.after.lines[i].data =
                        malloc_s((size_t)edit.before.lines[i].size);
                    memcpy(edit.after.lines[i].data, edit.before.lines[i].data,
                           (size_t)edit.before.lines[i].size);
                }
            }

            if (edit.after.size > 1) {
                if (move_up) {
                    Str temp = edit.after.lines[0];
                    memmove(&edit.after.lines[0], &edit.after.lines[1],
                            (edit.after.size - 1) * sizeof(Str));
                    edit.after.lines[edit.after.size - 1] = temp;
                } else {
                    Str temp = edit.after.lines[edit.after.size - 1];
                    memmove(&edit.after.lines[1], &edit.after.lines[0],
                            (edit.after.size - 1) * sizeof(Str));
                    edit.after.lines[0] = temp;
                }
            }

            should_set_edit_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = tab->cursor.is_selected;
            if (move_up) {
                new_cursor.y--;
                new_cursor.select_y--;
            } else {
                new_cursor.y++;
                new_cursor.select_y++;
            }
        } break;

        // Delete/backspace
        case DEL_KEY:
        case CTRL_KEY('h'):
        case BACKSPACE: {
            bool is_delete = (e == DEL_KEY);
            if (!tab->cursor.is_selected) {
                if (is_delete) {
                    if (tab->cursor.y == file->num_rows - 1 &&
                        tab->cursor.x == file->row[file->num_rows - 1].size)
                        break;
                } else if (tab->cursor.x == 0 && tab->cursor.y == 0) {
                    break;
                }
            }

            should_scroll = true;
            keep_bracket_autocomplete = true;
            has_edit = true;

            if (tab->cursor.is_selected) {
                EditorSelectRange range;
                editorGetSelectRange(&tab->cursor, &range);
                edit.x = range.start_x;
                edit.y = range.start_y;
                editorCopyText(file, &edit.before, range);
                editorFreeClipboardContent(&edit.after);
                should_set_edit_cursor = true;
                new_cursor = tab->cursor;
                new_cursor.is_selected = false;
                new_cursor.x = range.start_x;
                new_cursor.y = range.start_y;
                break;
            }

            int cx = tab->cursor.x;
            int cy = tab->cursor.y;
            int start_x = cx;
            int start_y = cy;
            int end_x = cx;
            int end_y = cy;
            int new_x = cx;
            int new_y = cy;
            int bracket_delta = 0;

            EditorRow* row = &file->row[cy];

            if (is_delete) {
                if (cx < row->size) {
                    end_x = editorRowNextUTF8(row, cx);
                } else if (cy + 1 < file->num_rows) {
                    end_x = 0;
                    end_y = cy + 1;
                }
            } else {
                if (cx > 0) {
                    start_x = editorRowPreviousUTF8(row, cx);
                    char deleted_char = row->data[start_x];
                    bool should_delete_tab =
                        backspace.int_value && deleted_char == ' ';
                    if (should_delete_tab) {
                        bool only_spaces = true;
                        for (int i = 0; i < start_x; i++) {
                            if (row->data[i] != ' ' && row->data[i] != '\t') {
                                only_spaces = false;
                                break;
                            }
                        }
                        if (only_spaces) {
                            int rx = editorRowCxToRx(row, start_x);
                            while (rx % tabsize.int_value != 0 && start_x > 0 &&
                                   row->data[start_x - 1] == ' ') {
                                start_x--;
                                rx--;
                            }
                        }
                    }
                    new_x = start_x;
                } else if (cy > 0) {
                    start_y = cy - 1;
                    start_x = file->row[start_y].size;
                    new_y = start_y;
                    new_x = start_x;
                }
            }

            if (tab->bracket_autocomplete && cx > 0 && cx < row->size) {
                char left = row->data[cx - 1];
                char right = row->data[cx];
                bool match = isCloseBracket(right) == left ||
                             (left == '\'' && right == '\'') ||
                             (left == '"' && right == '"');
                if (match && !is_delete) {
                    end_x = editorRowNextUTF8(row, cx);
                    start_x = editorRowPreviousUTF8(row, cx);
                    new_x = start_x;
                    bracket_delta = -1;
                } else if (match && is_delete) {
                    end_x = editorRowNextUTF8(row, cx);
                    bracket_delta = -1;
                }
            }

            EditorSelectRange range = {start_x, start_y, end_x, end_y};
            edit.x = range.start_x;
            edit.y = range.start_y;
            editorCopyText(file, &edit.before, range);
            editorFreeClipboardContent(&edit.after);
            tab->bracket_autocomplete += bracket_delta;
            if (tab->bracket_autocomplete < 0)
                tab->bracket_autocomplete = 0;
            should_set_edit_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            new_cursor.x = new_x;
            new_cursor.y = new_y;
        } break;

        // Newline
        case '\r': {
            should_scroll = true;
            keep_bracket_autocomplete = true;
            has_edit = true;

            edit.x = tab->cursor.x;
            edit.y = tab->cursor.y;

            EditorSelectRange delete_range = {0};
            if (tab->cursor.is_selected) {
                editorGetSelectRange(&tab->cursor, &delete_range);
                editorCopyText(file, &edit.before, delete_range);
            }

            editorFreeClipboardContent(&edit.after);
            editorClipboardAppendNewline(&edit.after);
            editorClipboardAppendNewline(&edit.after);

            if (autoindent.int_value) {
                const EditorRow* row = &file->row[tab->cursor.y];
                bool should_indent;
                if (tab->cursor.x < row->size) {
                    should_indent = false;
                    for (int i = tab->cursor.x; i < row->size; i++) {
                        if (row->data[i] != ' ' && row->data[i] != '\t') {
                            should_indent = true;
                            break;
                        }
                    }
                } else {
                    should_indent = true;
                }

                if (should_indent) {
                    int indent_limit = tab->cursor.x;
                    if (indent_limit > row->size)
                        indent_limit = row->size;

                    int i = 0;
                    while (i < indent_limit &&
                           (row->data[i] == ' ' || row->data[i] == '\t')) {
                        i++;
                    }
                    if (i > 0) {
                        editorClipboardAppendAt(&edit.after, 1, row->data,
                                                (size_t)i);
                    }

                    // TODO: language specific auto indent
                    bool should_inc = false;
                    if (tab->cursor.x > 0) {
                        char prev = row->data[tab->cursor.x - 1];
                        if (prev == ':') {
                            // Python
                            should_inc = true;
                        } else if (prev == '{') {
                            // C
                            if (tab->cursor.x < row->size) {
                                should_inc = (row->data[tab->cursor.x] != '}');
                            } else {
                                should_inc = true;
                            }
                        }
                    }

                    if (should_inc) {
                        if (whitespace.int_value) {
                            editorClipboardAppendAtRepeat(
                                &edit.after, edit.after.size - 1, ' ',
                                tabsize.int_value);
                        } else {
                            editorClipboardAppendChar(&edit.after, '\t');
                        }
                    }
                }
            }

            should_set_edit_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            new_cursor.x = edit.after.lines[1].size;
            new_cursor.y = tab->cursor.y + 1;
        } break;

        // Key input
        case CHAR_INPUT: {
            should_scroll = true;
            keep_bracket_autocomplete = true;
            has_edit = true;

            uint32_t c = input.data.unicode;
            EditorSelectRange delete_range = {
                tab->cursor.x,
                tab->cursor.y,
                tab->cursor.x,
                tab->cursor.y,
            };

            if (tab->cursor.is_selected) {
                editorGetSelectRange(&tab->cursor, &delete_range);
                editorCopyText(file, &edit.before, delete_range);
                tab->cursor.is_selected = false;
            }

            edit.x = delete_range.start_x;
            edit.y = delete_range.start_y;
            editorFreeClipboardContent(&edit.after);

            int close_bracket = isOpenBracket(c);
            int open_bracket = isCloseBracket(c);
            bool should_skip = false;
            bool did_autocomplete = false;
            if (c == '\t' && whitespace.int_value) {
                int tab_size = tabsize.int_value;
                int column =
                    editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
                int total_spaces = tab_size - (column % tab_size);
                if (total_spaces <= 0)
                    total_spaces = tab_size;

                editorClipboardAppendAtRepeat(&edit.after, 0, ' ',
                                              (size_t)total_spaces);
            } else if (!bracket.int_value) {
                editorClipboardAppendUnicode(&edit.after, c);
            } else if (close_bracket) {
                editorClipboardAppendUnicode(&edit.after, c);
                editorClipboardAppendChar(&edit.after, close_bracket);
                did_autocomplete = true;
                tab->bracket_autocomplete++;
            } else if (open_bracket) {
                if (tab->bracket_autocomplete &&
                    file->row[tab->cursor.y].data[tab->cursor.x] == (int)c) {
                    tab->bracket_autocomplete--;
                    should_skip = true;
                } else {
                    editorClipboardAppendUnicode(&edit.after, c);
                }
            } else if (c == '\'' || c == '"') {
                if (file->row[tab->cursor.y].data[tab->cursor.x] != (int)c) {
                    editorClipboardAppendUnicode(&edit.after, c);
                    editorClipboardAppendChar(&edit.after, c);
                    did_autocomplete = true;
                    tab->bracket_autocomplete++;
                } else if (tab->bracket_autocomplete &&
                           file->row[tab->cursor.y].data[tab->cursor.x] ==
                               (int)c) {
                    tab->bracket_autocomplete--;
                    should_skip = true;
                } else {
                    editorClipboardAppendUnicode(&edit.after, c);
                }
            } else {
                editorClipboardAppendUnicode(&edit.after, c);
            }

            if (tab->bracket_autocomplete < 0)
                tab->bracket_autocomplete = 0;

            should_set_edit_cursor = true;
            new_cursor = tab->cursor;
            new_cursor.is_selected = false;
            if (should_skip) {
                has_edit = false;
                tab->cursor.x++;
            } else if (did_autocomplete) {
                new_cursor.x = edit.x + edit.after.lines[0].size - 1;
                new_cursor.y = edit.y;
            } else {
                new_cursor.x = edit.x + edit.after.lines[0].size;
                new_cursor.y = edit.y;
            }
        } break;

        default:
            // Do nothing
            return;
    }

    // File may have been closed or changed
    if (!tab || !file)
        return;

    if (has_edit && file->read_only && !file->unlocked) {
        editorMsgClear();
        editorMsg("File is read-only.");

        editorFreeClipboardContent(&edit.before);
        editorFreeClipboardContent(&edit.after);
        has_edit = false;
    }

    if (has_edit) {
        editorApplyEdit(tab, &edit, false);
        if (should_set_edit_cursor) {
            keep_selection = true;
            tab->cursor = new_cursor;
            editorUpdateSx(tab);
        }

        EditorAction* action = calloc_s(1, sizeof(EditorAction));
        action->type = ACTION_EDIT;
        EditAction* edit_action = &action->edit;
        edit_action->data = edit;
        edit_action->old_cursor = old_cursor;
        edit_action->new_cursor = tab->cursor;
        editorAppendAction(file, action);
    }

    if (!keep_selection) {
        tab->cursor.is_selected = false;
    }

    if (tab->cursor.x == tab->cursor.select_x &&
        tab->cursor.y == tab->cursor.select_y) {
        tab->cursor.is_selected = false;
    }

    if (!tab->cursor.is_selected) {
        tab->cursor.select_x = tab->cursor.x;
        tab->cursor.select_y = tab->cursor.y;
    }

    if (!keep_bracket_autocomplete) {
        tab->bracket_autocomplete = 0;
    }

    if (should_scroll) {
        editorScrollToCursor(p);
    }
}

static void editorUpdateTabDisplayed(EditPanel* split) {
    if (!split)
        return;

    int w = split->base.layout->rect.w;
    split->tab_displayed =
        iterateDisplayTabs(split, w, NULL, NULL).tab_displayed;
}

typedef enum TabClickResult {
    TAB_CLICK_NONE,
    TAB_CLICK_LEFT_INDICATOR,
    TAB_CLICK_RIGHT_INDICATOR,
    TAB_CLICK_INDEX,
} TabClickResult;

typedef struct TabClickData {
    int x;
    int tab_index;
} TabClickData;

static void tabClickCallback(int index,
                             int x,
                             int display_width,
                             const char* display_name,
                             bool active_tab,
                             void* user_data) {
    UNUSED(display_name);
    UNUSED(active_tab);

    TabClickData* data = (TabClickData*)user_data;
    if (data->x >= x && data->x < x + display_width) {
        data->tab_index = index;
    }
}

static TabClickResult getTabClick(const EditPanel* split,
                                  int x,
                                  int* out_index) {
    if (!split)
        return TAB_CLICK_NONE;

    TabClickData data = {.x = x, .tab_index = -1};
    TabDisplayResult result = iterateDisplayTabs(
        split, split->base.layout->rect.w, tabClickCallback, &data);
    if (out_index)
        *out_index = data.tab_index;
    if (data.tab_index != -1)
        return TAB_CLICK_INDEX;

    // Right indicator draw last, so take piority over left indicator
    if (result.right_indicator_x != -1 && x == result.right_indicator_x)
        return TAB_CLICK_RIGHT_INDICATOR;
    if (result.left_indicator_x != -1 && x == result.left_indicator_x)
        return TAB_CLICK_LEFT_INDICATOR;

    return TAB_CLICK_NONE;
}

static void scrollTabBar(EditPanel* split, bool scroll_left) {
    if (!split)
        return;

    if (scroll_left) {
        if (split->tab_offset > 0) {
            split->tab_offset--;
        }
    } else {
        if (split->tab_offset + split->tab_displayed < (int)split->tabs.size) {
            split->tab_offset++;
        }
    }
    editorUpdateTabDisplayed(split);
}

static void handleTabBarPress(EditPanel* split, UIMouseEvent mouse_event) {
    if (!split || mouse_event.local_y != 0)
        return;

    int tab_index;
    TabClickResult click_result =
        getTabClick(split, mouse_event.local_x, &tab_index);

    if (click_result == TAB_CLICK_INDEX && tab_index != -1) {
        editorChangeToFile(split, tab_index);
    } else if (click_result == TAB_CLICK_LEFT_INDICATOR) {
        scrollTabBar(split, true);
    } else if (click_result == TAB_CLICK_RIGHT_INDICATOR) {
        scrollTabBar(split, false);
    }
}

static void handleTabBarClose(EditPanel* split,
                              UIMouseEvent mouse_event,
                              EditWaitState wait_state) {
    if (!split)
        return;

    UIMouseEventType type = mouse_event.state->type;
    if (type != UI_MOUSE3_PRESSED && type != UI_MOUSE3_RELEASED) {
        return;
    }

    bool pressed = (type == UI_MOUSE3_PRESSED);

    int tab_index;
    TabClickResult click_result =
        getTabClick(split, mouse_event.local_x, &tab_index);

    if (click_result == TAB_CLICK_INDEX && tab_index != -1) {
        // Handle tab close confirmation
        switch (wait_state) {
            case EDIT_WAIT_NONE:
                if (pressed) {
                    split->wait_state = EDIT_WAIT_CLOSE_RELEASE1;
                    split->wait_tab_index = tab_index;
                    gEditor.pending_edit_panel = split;
                }
                break;

            case EDIT_WAIT_CLOSE_RELEASE1:
                if (!pressed) {
                    if (split->wait_tab_index != tab_index) {
                        editorCancelPendingWait(split);
                        break;
                    }

                    const EditorFile* file =
                        editorTabGetFileConst(&split->tabs.data[tab_index]);
                    if (file->dirty && file->reference_count == 1) {
                        split->wait_state = EDIT_WAIT_CLOSE_PRESS2;

                        editorMsgClear();
                        editorMsg("File has unsaved changes.");
                        editorMsg("Press close again to close file anyway.");
                    } else {
                        editorCloseTab(split, tab_index);
                        editorCancelPendingWait(split);
                    }
                }
                break;

            case EDIT_WAIT_CLOSE_PRESS2:
                if (pressed) {
                    if (split->wait_tab_index != tab_index) {
                        editorCancelPendingWait(split);
                        break;
                    }

                    gEditor.con_keep_msg = true;
                    split->wait_state = EDIT_WAIT_CLOSE_RELEASE2;
                }
                break;

            case EDIT_WAIT_CLOSE_RELEASE2:
                if (!pressed) {
                    editorMsgClear();

                    if (split->wait_tab_index != tab_index) {
                        editorCancelPendingWait(split);
                        break;
                    }

                    editorCloseTab(split, tab_index);
                    editorCancelPendingWait(split);
                }
                break;

            default:
                break;
        }
    }
}

static bool handleSelectClick(EditPanel* split, UIMouseEvent mouse_event) {
    if (!split)
        return false;

    int x = mouse_event.local_x;
    int y = mouse_event.local_y;

    EditorTab* tab = editorSplitGetTab(split);
    const EditorFile* file = editorTabGetFile(tab);

    const int tab_bar_height = 1;
    const int lineno_width = lineno.int_value ? file->lineno_width : 0;
    if (x < lineno_width) {
        editorSelectLine(tab, tab->row_offset + y - tab_bar_height);
        return true;
    }

    int cx, cy;
    editorMousePosToEditorPos(split, x, y, &cx, &cy);

    if (cy < 0 || cy >= file->num_rows) {
        return false;
    }

    switch (mouse_event.state->click_count % 4) {
        case 1:
            tab->cursor.x = cx;
            tab->cursor.y = cy;
            tab->cursor.select_x = cx;
            tab->cursor.select_y = cy;
            // Start selection will happen on mouse move
            tab->cursor.is_selected = false;
            editorUpdateSx(tab);
            break;

        case 2: {
            // Select word
            const EditorRow* row = &file->row[cy];
            if (row->size == 0)
                break;
            if (cx == row->size)
                cx--;

            IsCharFunc is_char;
            if (isSpace(row->data[cx])) {
                is_char = isNonSpace;
            } else if (isIdentifierChar(row->data[cx])) {
                is_char = isNonIdentifierChar;
            } else {
                is_char = isNonSeparator;
            }
            editorSelectWord(tab, row, cx, is_char);
        } break;

        case 3:
            // Select line
            editorSelectLine(tab, tab->cursor.y);
            break;

        case 0:
            // Select all
            editorSelectAll(tab);
            break;
    }

    return true;
}

void handleSelectDrag(EditPanel* split, UIMouseEvent mouse_event) {
    if (!split)
        return;

    int x = mouse_event.local_x;
    int y = mouse_event.local_y;

    EditorTab* tab = editorSplitGetTab(split);
    EditorFile* file = editorTabGetFile(tab);

    const int lineno_width = lineno.int_value ? file->lineno_width : 0;
    if (x < lineno_width) {
        y++;
    }

    int cx, cy;
    editorMousePosToEditorPos(split, x, y, &cx, &cy);

    if (cy < 0 || cy >= file->num_rows) {
        return;
    }

    tab->cursor.x = cx;
    tab->cursor.y = cy;

    if (tab->cursor.x != tab->cursor.select_x ||
        tab->cursor.y != tab->cursor.select_y) {
        tab->cursor.is_selected = true;
    }
}

static bool mouseEvent(Panel* self, UIMouseEvent mouse_event) {
    EditPanel* p = (EditPanel*)self;
    UIMouseEventType type = mouse_event.state->type;

    EditWaitState wait_state = p->wait_state;
    p->wait_state = EDIT_WAIT_NONE;

    const int tab_bar_height = 1;
    bool on_tab_bar = (mouse_event.local_y < tab_bar_height);

    EditorTab* tab = editorSplitGetTab(p);
    if (tab) {
        tab->bracket_autocomplete = 0;
    }

    switch (type) {
        case UI_MOUSE1_PRESSED:
            if (on_tab_bar) {
                p->mouse_mode = EDIT_MOUSE_TAB_BAR;
                handleTabBarPress(p, mouse_event);
                return false;
            }
            p->mouse_mode = EDIT_MOUSE_SELECT_DRAG;
            return handleSelectClick(p, mouse_event);

        case UI_MOUSE1_MOVE:
            if (p->mouse_mode == EDIT_MOUSE_SELECT_DRAG)
                handleSelectDrag(p, mouse_event);
            return p->mouse_mode == EDIT_MOUSE_SELECT_DRAG;

        case UI_MOUSE1_RELEASED:
            p->mouse_mode = EDIT_MOUSE_NONE;
            return false;

        case UI_MOUSE3_PRESSED:
        case UI_MOUSE3_RELEASED:
            if (on_tab_bar)
                handleTabBarClose(p, mouse_event, wait_state);
            return false;

        case UI_MWHEEL_UP:
        case UI_MWHEEL_DOWN:
            if (on_tab_bar) {
                scrollTabBar(p, type == UI_MWHEEL_UP);
            } else {
                editorScroll(p, type == UI_MWHEEL_UP ? -EDITOR_SCROLL_DIST
                                                     : EDITOR_SCROLL_DIST);
            }
            return false;

        default:
            return false;
    }
}

int editorAddFileToActiveSplit(EditorFile* file) {
    int file_index = editorAddFile(file);
    if (file_index != -1) {
        EditPanel* split = gEditor.active_edit_panel;
        if (!split) {
            split = editorAddSplit(NULL, true);
            gEditor.active_edit_panel = split;
        }

        int tab_index = editorAddTab(split, file_index);
        if (tab_index != -1) {
            return file_index;
        }
        editorRemoveFile(file_index);
    }
    return -1;
}

int editorAddTab(EditPanel* split, int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return -1;
    if (!split)
        return -1;

    EditorFile* file = &gEditor.files[file_index];
    EditorTab tab = {0};
    tab.file_index = file_index;

    if (file->reference_count == 0) {
        gEditor.file_count++;
    }
    file->reference_count++;

    vector_push(split->tabs, tab);

    int index = split->tabs.size - 1;
    editorChangeToFile(split, index);

    return index;
}

// Won't update tab_active_index
static void editorRemoveTab(EditPanel* split, int tab_index) {
    if (!split)
        return;

    if (tab_index < 0 || (uint32_t)tab_index >= split->tabs.size)
        return;

    int file_index = split->tabs.data[tab_index].file_index;
    editorRemoveFile(file_index);

    vector_erase(split->tabs, (uint32_t)tab_index);
}

void editorCloseTab(EditPanel* split, int tab_index) {
    if (!split)
        return;

    if (tab_index < 0 || (uint32_t)tab_index >= split->tabs.size)
        return;

    editorRemoveTab(split, tab_index);
    // Close split if no file in the tab
    if (split->tabs.size == 0) {
        editorRemoveSplit(split);
        if (gEditor.split_count == 0) {
            if (!gEditor.explorer_panel->node) {
                gEditor.state = STATE_EXIT;
            } else {
                uiPanelSetEnabled(&gEditor.ui, (Panel*)gEditor.explorer_panel,
                                  true);
                uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.explorer_panel);
            }
        }
    } else {
        int new_tab_index = split->tab_active_index;
        if (tab_index < new_tab_index) {
            new_tab_index--;
        } else if (tab_index == new_tab_index &&
                   new_tab_index >= (int)split->tabs.size) {
            new_tab_index = split->tabs.size - 1;
        }
        editorChangeToFile(split, new_tab_index);
    }
}

int editorFindTabByFileIndex(EditPanel* split, int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return -1;
    if (!split)
        return -1;

    for (uint32_t i = 0; i < split->tabs.size; i++) {
        if (split->tabs.data[i].file_index == file_index) {
            return i;
        }
    }
    return -1;
}

void editorChangeToFile(EditPanel* split, int tab_index) {
    if (!split)
        return;

    if (tab_index < 0 || (uint32_t)tab_index >= split->tabs.size)
        return;

    editorUpdateTabDisplayed(split);

    split->tab_active_index = tab_index;

    if (split->tab_offset > tab_index ||
        split->tab_offset + split->tab_displayed <= tab_index) {
        split->tab_offset = tab_index;
    }

    EditorTab* tab = &split->tabs.data[tab_index];
    tab->bracket_autocomplete = 0;
}

// Do not focus the split
EditPanel* editorAddSplit(EditPanel* relative_to, bool leftright) {
    EditPanel* new_split = panelEditCreate();

    if (!relative_to) {
        // Create a left-right layout with explorer and welcome panel
        uiAddPanel(&gEditor.ui, ((Panel*)gEditor.welcome_panel),
                   (Panel*)new_split, true);
        uiPanelSetEnabled(&gEditor.ui, (Panel*)gEditor.welcome_panel, false);
    } else {
        uiAddPanel(&gEditor.ui, ((Panel*)relative_to), (Panel*)new_split,
                   leftright);
    }

    gEditor.split_count++;
    return new_split;
}

void editorRemoveSplit(EditPanel* split) {
    if (!split)
        return;

    if (gEditor.active_edit_panel == split) {
        gEditor.active_edit_panel = NULL;
    }

    uiClosePanel(&gEditor.ui, (Panel*)split);
    gEditor.split_count--;

    if (gEditor.split_count == 0) {
        uiPanelSetEnabled(&gEditor.ui, (Panel*)gEditor.welcome_panel, true);
    }
}

void editorScroll(EditPanel* split, int dist) {
    EditorTab* tab = editorSplitGetTab(split);
    const EditorFile* file = editorTabGetFile(tab);
    if (!file)
        return;

    int line = tab->row_offset + dist;
    if (line < 0) {
        line = 0;
    } else if (line >= file->num_rows) {
        line = file->num_rows - 1;
    }
    tab->row_offset = line;
}

void editorScrollToCursor(EditPanel* split) {
    if (!split)
        return;

    EditorTab* tab = editorSplitGetTab(split);
    const EditorFile* file = editorTabGetFile(tab);
    if (!file)
        return;

    const int tab_bar_height = 1;
    int w = split->base.layout->rect.w;
    int h = split->base.layout->rect.h - tab_bar_height;  // Exclude tab bar
    int lineno_width = lineno.int_value ? file->lineno_width : 0;
    int cols = w - lineno_width;

    int rx = 0;
    if (tab->cursor.y < file->num_rows) {
        rx = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
    }

    if (tab->cursor.y < tab->row_offset) {
        tab->row_offset = tab->cursor.y;
    }
    if (tab->cursor.y >= tab->row_offset + h) {
        tab->row_offset = tab->cursor.y - h + 1;
    }
    if (rx < tab->col_offset) {
        tab->col_offset = rx;
    }
    if (rx >= tab->col_offset + cols) {
        tab->col_offset = rx - cols + 1;
    }
}

void editorScrollToCursorCenter(EditPanel* split) {
    if (!split)
        return;

    EditorTab* tab = editorSplitGetTab(split);
    if (!tab)
        return;

    tab->row_offset = tab->cursor.y - split->base.layout->rect.h / 2;
    if (tab->row_offset < 0) {
        tab->row_offset = 0;
    }
}

void editorCancelPendingWait(EditPanel* split) {
    if (!split)
        return;

    if (split == gEditor.pending_edit_panel)
        gEditor.pending_edit_panel = NULL;

    split->wait_state = EDIT_WAIT_NONE;
    split->wait_tab_index = -1;
}
