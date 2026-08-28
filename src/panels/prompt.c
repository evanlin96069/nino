#include "panels/prompt.h"

#include <stdarg.h>

#include "editor.h"
#include "unicode.h"

static void destroy(Panel* self);
static void render(Panel* self, Surface s);
static bool getCursor(Panel* self, UICursor* out);
static void onFocus(Panel* self, bool focused);
static void keyEvent(Panel* self, KeyEvent event);
static bool mouseEvent(Panel* self, UIMouseEvent event);

static PanelVtable panel_vt = {
    .destroy = destroy,
    .render = render,
    .getCursor = getCursor,
    .onFocus = onFocus,
    .keyEvent = keyEvent,
    .mouseEvent = mouseEvent,
};

PromptPanel* panelPromptCreate(void) {
    PromptPanel* p = calloc_s(1, sizeof(PromptPanel));
    p->base.vt = &panel_vt;
    p->base.kind = PANEL_KIND_PROMPT;
    p->select_x = -1;
    return p;
}

static void destroy(Panel* self) {
    PromptPanel* p = (PromptPanel*)self;
    editorFreeRow(&p->prompt_row);
}

static inline void getSelectStartEnd(PromptPanel* p,
                                     int* out_start,
                                     int* out_end) {
    if (p->select_x < 0 || p->select_x == p->cx) {
        *out_start = -1;
        *out_end = -1;
    } else {
        *out_start = p->select_x < p->cx ? p->select_x : p->cx;
        *out_end = p->select_x > p->cx ? p->select_x : p->cx;
    }
}

static void render(Panel* self, Surface s) {
    PromptPanel* p = (PromptPanel*)self;
    if (s.h != 1 || s.w <= 0) {
        return;
    }

    ScreenCell* row = SURFACE_ROW(s, 0);
    ScreenStyle style = {
        .fg = gEditor.color_cfg[UI_COLOR_PROMPT_FG],
        .bg = gEditor.color_cfg[UI_COLOR_PROMPT_BG],
    };

    screenClearCells(row, s.w, 0, s.w, style);

    const char* prefix = p->prompt_prefix;
    const char* right = p->prompt_right;

    // Right prompt is currently only used by find mode, assume it's ASCII
    int rlen = strlen(right);
    if (rlen > s.w) {
        rlen = 0;
    }

    // Draw prefix (ASCII-only)
    int prefix_width = screenPutAscii(row, s.w, 0, prefix, style);
    int x = prefix_width;

    // Draw prompt
    const EditorRow* prompt_row = &p->prompt_row;
    if (prompt_row->data && prompt_row->size > 0) {
        const char* curr = prompt_row->data;
        int remaining = prompt_row->size;
        int rx = 0;  // relative to prompt row start (for tabs)

        while (remaining > 0 && x < s.w) {
            if (*curr == '\t') {
                int tab_size = tabsize.int_value;
                int spaces = tab_size - (rx % tab_size);
                for (int i = 0; i < spaces && x < s.w; i++) {
                    x += screenPutChar(row, s.w, x, ' ', &style);
                    rx++;
                }
                curr++;
                remaining--;
            } else {
                size_t byte_size;
                uint32_t code_point = decodeUTF8(curr, remaining, &byte_size);

                if (byte_size == 0)
                    break;

                int width = unicodeWidth(code_point);
                if (width <= 0) {
                    // Skip zero-width or invalid characters
                    curr += byte_size;
                    remaining -= byte_size;
                    continue;
                }

                if (x + width > s.w)
                    break;

                // Build grapheme cluster
                Grapheme grapheme = {0};
                grapheme.cluster[0] = code_point;
                grapheme.size = 1;
                grapheme.width = width;

                curr += byte_size;
                remaining -= byte_size;

                // Gather trailing zero-width characters
                while (grapheme.size < MAX_CLUSTER_SIZE && remaining > 0) {
                    size_t comb_byte_size;
                    uint32_t comb_code_point =
                        decodeUTF8(curr, remaining, &comb_byte_size);

                    int comb_width = unicodeWidth(comb_code_point);
                    if (comb_width != 0)
                        break;

                    grapheme.cluster[grapheme.size] = comb_code_point;
                    grapheme.size++;

                    curr += comb_byte_size;
                    remaining -= comb_byte_size;
                }

                x += screenPutGrapheme(row, s.w, x, &grapheme, &style);
                rx += width;
            }
        }
    }

    // Selection highlight
    int select_start, select_end;
    getSelectStartEnd(p, &select_start, &select_end);
    select_start += prefix_width;
    select_end += prefix_width;
    if (select_start != -1 && select_end != -1) {
        ScreenStyle select_style = {
            .fg = style.fg,
            .bg = gEditor.color_cfg[UI_COLOR_HL_SELECT],
        };
        if (select_end > s.w)
            select_end = s.w;
        for (int i = select_start; i < select_end; i++) {
            if (!row[i].continuation)
                row[i].style.bg = select_style.bg;
        }
    }

    if (x < s.w - rlen) {
        screenPutAscii(row, s.w, s.w - rlen, right, style);
    }
}

static bool getCursor(Panel* self, UICursor* out) {
    PromptPanel* p = (PromptPanel*)self;

    bool visible = false;
    int out_x = 0;
    int out_y = 0;

    int x = p->prompt_prefix_len + editorRowCxToRx(&p->prompt_row, p->cx);
    if (x >= 0 && x < self->layout->rect.w) {
        visible = true;
        out_x = x;
    }

    if (out) {
        out->visible = visible;
        out->x = out_x;
        out->y = out_y;
    }

    return visible;
}

static void onFocus(Panel* self, bool focused) {
    PromptPanel* p = (PromptPanel*)self;
    if (!focused && p->base.layout->enabled) {
        if (p->callback) {
            PromptEvent event = {
                .type = PROMPT_EVENT_CANCEL,
                .query = p->prompt_row.data,
            };
            p->callback(event, p->user_data);
        }
        uiPanelSetEnabled(&gEditor.ui, self, false);
    }
}

static void keyEvent(Panel* self, KeyEvent event) {
    PromptPanel* p = (PromptPanel*)self;
    EditorRow* row = &p->prompt_row;

    int select_start, select_end;
    getSelectStartEnd(p, &select_start, &select_end);
    const bool is_selected = (select_start != -1 && select_end != -1);

    switch (event.value) {
        // Cancel event
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'Q'):
        case KEYVAL(KEY_ESC):
            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_CANCEL,
                    .query = row->data,
                };
                p->callback(ev, p->user_data);
            }
            uiPanelSetEnabled(&gEditor.ui, self, false);
            if (gEditor.ui.focused_panel == self) {
                // The callback didn't change the focus
                uiPanelSetFocused(&gEditor.ui, gEditor.ui.last_focused_panel);
            }
            break;

        // Submit event
        case KEYVAL(KEY_ENTER):
            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_SUBMIT,
                    .query = row->data,
                };
                p->callback(ev, p->user_data);
            }
            uiPanelSetEnabled(&gEditor.ui, self, false);
            if (gEditor.ui.focused_panel == self) {
                // The callback didn't change the focus
                uiPanelSetFocused(&gEditor.ui, gEditor.ui.last_focused_panel);
            }
            break;

        // Key events
        case KEYVAL(KEY_TEXT): {
            char output[4];
            int len = encodeUTF8(event.unicode, output);
            if (len == -1)
                break;

            if (is_selected) {
                editorRowDeleteRange(NULL, row, select_start, select_end);
                p->cx = select_start;
                p->select_x = -1;
            }
            editorRowInsertString(NULL, row, p->cx, output, len);
            p->cx += len;

            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_KEY,
                    .query = row->data,
                    .key_event = event,
                };
                p->callback(ev, p->user_data);
            }
        } break;

        case KEYVAL(KEY_TAB):
            // TODO: Handle tab completion
            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_KEY,
                    .query = row->data,
                    .key_event = event,
                };
                p->callback(ev, p->user_data);
            }
            break;

        case KEYVAL(KEY_DELETE):
            if (is_selected) {
                editorRowDeleteRange(NULL, row, select_start, select_end);
                p->cx = select_start;
                p->select_x = -1;
            } else if (p->cx < row->size) {
                int next = editorRowNextUTF8(row, p->cx);
                editorRowDeleteRange(NULL, row, p->cx, next);
            }

            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_KEY,
                    .query = row->data,
                    .key_event = event,
                };
                p->callback(ev, p->user_data);
            }
            break;

        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'H'):
        case KEYVAL(KEY_BACKSPACE):
            if (is_selected) {
                editorRowDeleteRange(NULL, row, select_start, select_end);
                p->cx = select_start;
                p->select_x = -1;
            } else {
                int prev = editorRowPreviousUTF8(row, p->cx);
                editorRowDeleteRange(NULL, row, prev, p->cx);
                p->cx = prev;
            }

            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_KEY,
                    .query = row->data,
                    .key_event = event,
                };
                p->callback(ev, p->user_data);
            }
            break;

        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'V'): {
            const EditorClipboard* clipboard = &gEditor.clipboard;
            if (!clipboard->size)
                break;

            // Only paste the first line
            const char* paste_buf = clipboard->lines[0].data;
            size_t paste_len = clipboard->lines[0].size;
            if (paste_len == 0)
                break;

            if (is_selected) {
                editorRowDeleteRange(NULL, row, select_start, select_end);
                p->cx = select_start;
                p->select_x = -1;
            }
            editorRowInsertString(NULL, row, p->cx, paste_buf, paste_len);
            p->cx += (int)paste_len;

            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_KEY,
                    .query = row->data,
                    .key_event = event,
                };
                p->callback(ev, p->user_data);
            }
        } break;

        case KEYVAL(KEY_UP):
        case KEYVAL(KEY_DOWN):
            // Find feature uses this
            if (p->callback) {
                editorRowEnsureNull(row);
                PromptEvent ev = {
                    .type = PROMPT_EVENT_KEY,
                    .query = row->data,
                    .key_event = event,
                };
                p->callback(ev, p->user_data);
            }
            break;

        case KEYVAL(KEY_MOD_SHIFT, KEY_HOME):
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = 0;
            break;

        case KEYVAL(KEY_HOME):
            p->cx = 0;
            p->select_x = -1;
            break;

        case KEYVAL(KEY_MOD_SHIFT, KEY_END):
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = row->size;
            break;

        case KEYVAL(KEY_END):
            p->cx = row->size;
            p->select_x = -1;
            break;

        case KEYVAL(KEY_MOD_SHIFT, KEY_LEFT):
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = editorRowPreviousUTF8(row, p->cx);
            break;

        case KEYVAL(KEY_LEFT):
            if (is_selected) {
                p->cx = p->cx < p->select_x ? p->cx : p->select_x;
                p->select_x = -1;
            } else {
                p->cx = editorRowPreviousUTF8(row, p->cx);
            }
            break;

        case KEYVAL(KEY_MOD_SHIFT, KEY_RIGHT):
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = editorRowNextUTF8(row, p->cx);
            break;

        case KEYVAL(KEY_RIGHT):
            if (is_selected) {
                p->cx = p->cx > p->select_x ? p->cx : p->select_x;
                p->select_x = -1;
            } else {
                p->cx = editorRowNextUTF8(row, p->cx);
            }
            break;

        case KEYVAL(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_LEFT):
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = editorRowWordLeft(row, p->cx);
            break;

        case KEYVAL(KEY_MOD_CTRL, KEY_LEFT):
            p->cx = editorRowWordLeft(row, p->cx);
            p->select_x = -1;
            break;

        case KEYVAL(KEY_MOD_SHIFT | KEY_MOD_CTRL, KEY_RIGHT):
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = editorRowWordRight(row, p->cx);
            break;

        case KEYVAL(KEY_MOD_CTRL, KEY_RIGHT):
            p->cx = editorRowWordRight(row, p->cx);
            p->select_x = -1;
            break;

        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'A'):
            if (row->size > 0) {
                p->select_x = 0;
                p->cx = row->size;
            }
            break;

        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'C'):
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'X'): {
            if (!is_selected)
                break;

            editorFreeClipboardContent(&gEditor.clipboard);
            gEditor.clipboard.size = 1;
            gEditor.clipboard.lines = calloc_s(1, sizeof(Str));
            gEditor.clipboard.lines[0].data =
                malloc_s(select_end - select_start);
            memcpy(gEditor.clipboard.lines[0].data, &row->data[select_start],
                   select_end - select_start);
            gEditor.clipboard.lines[0].size = select_end - select_start;
            gEditor.copy_line = false;

            if (event.value == KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'X')) {
                editorRowDeleteRange(NULL, row, select_start, select_end);
                p->cx = select_start;
                p->select_x = -1;

                if (p->callback) {
                    editorRowEnsureNull(row);
                    PromptEvent ev = {
                        .type = PROMPT_EVENT_KEY,
                        .query = row->data,
                        .key_event = event,
                    };
                    p->callback(ev, p->user_data);
                }
            }
        } break;
    }
}

static bool mouseEvent(Panel* self, UIMouseEvent event) {
    PromptPanel* p = (PromptPanel*)self;
    EditorRow* row = &p->prompt_row;

    int select_start, select_end;
    getSelectStartEnd(p, &select_start, &select_end);
    const bool is_selected = (select_start != -1 && select_end != -1);

    int mouse_cx = 0;
    if (event.mouse.x >= p->prompt_prefix_len) {
        mouse_cx = editorRowRxToCx(&p->prompt_row,
                                   event.mouse.x - p->prompt_prefix_len);
    }

    switch (event.mouse.type) {
        case MOUSE1_PRESSED: {
            if (event.mouse.y != 0) {
                return false;
            }

            switch (event.state->click_count % 3) {
                case 1:
                    // Mouse to pos
                    p->cx = mouse_cx;
                    p->select_x = -1;
                    break;

                case 2: {
                    // Select word
                    if (row->size == 0)
                        break;
                    int wcx = mouse_cx;
                    if (wcx >= row->size)
                        wcx = row->size > 0 ? row->size - 1 : 0;

                    IsCharFunc is_char;
                    if (isSpace(row->data[wcx])) {
                        is_char = isNonSpace;
                    } else if (isIdentifierChar(row->data[wcx])) {
                        is_char = isNonIdentifierChar;
                    } else {
                        is_char = isNonSeparator;
                    }
                    editorRowSelectWord(row, wcx, is_char, &p->select_x,
                                        &p->cx);
                } break;

                case 0:
                    // Select all
                    if (row->size > 0) {
                        p->select_x = 0;
                        p->cx = row->size;
                    }
                    break;

                default:
                    break;
            }

            return true;
        }

        case MOUSE1_DRAG:
            if (!is_selected) {
                p->select_x = p->cx;
            }
            p->cx = mouse_cx;
            break;

        default:
            break;
    }

    return false;
}

void editorPrompt(const char* prefix,
                  PromptCallback callback,
                  void* user_data) {
    PromptPanel* p = gEditor.prompt_panel;
    p->callback = callback;
    p->user_data = user_data;

    p->prompt_prefix[0] = '\0';
    if (prefix) {
        snprintf(p->prompt_prefix, sizeof(p->prompt_prefix), "%s", prefix);
        p->prompt_prefix_len = strlen(p->prompt_prefix);
    }
    p->prompt_right[0] = '\0';

    p->prompt_row.size = 0;
    p->cx = 0;
    p->select_x = -1;

    uiPanelSetEnabled(&gEditor.ui, (Panel*)p, true);
    uiPanelSetFocused(&gEditor.ui, (Panel*)p);
}

void editorSetRightPrompt(const char* fmt, ...) {
    PromptPanel* p = gEditor.prompt_panel;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->prompt_right, sizeof(p->prompt_right), fmt, ap);
    va_end(ap);
}
