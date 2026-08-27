#include "panels/welcome.h"

#include "config.h"
#include "editor.h"
#include "unicode.h"

static void destroy(Panel* self);
static void render(Panel* self, Surface s);
static bool getCursor(Panel* self, UICursor* out);
static void onFocus(Panel* self, bool focused);
static void keyEvent(Panel* self, KeyEvent event);
static bool mouseEvent(Panel* self, UIMouseEvent mouse_event);

static PanelVtable panel_vt = {
    .destroy = destroy,
    .render = render,
    .getCursor = getCursor,
    .onFocus = onFocus,
    .keyEvent = keyEvent,
    .mouseEvent = mouseEvent,
};

WelcomePanel* panelWelcomeCreate(void) {
    WelcomePanel* p = calloc_s(1, sizeof(WelcomePanel));
    p->base.vt = &panel_vt;
    p->base.kind = PANEL_KIND_WELCOME;
    return p;
}

static void destroy(Panel* self) {
    UNUSED(self);
}

static void render(Panel* self, Surface s) {
    UNUSED(self);

    if (s.h <= 0 || s.w <= 0) {
        return;
    }

    static int max_width = -1;

    // TODO: Display loading message when gEditor.state == STATE_LOADING
    const char* lines[] = {
        (EDITOR_NAME " v" EDITOR_VERSION),
        "",
        "by Evan Lin (evanline96069)",
        "https://github.com/evanlin96069/nino",
        "",
        "Save poor takos in the void!",
        "",
        "Ctrl+N  New Tab    Ctrl+O  Open",
        "Ctrl+P  Prompt     Ctrl+Q  Quit",
    };
    static int widths[sizeof(lines) / sizeof(lines[0])] = {0};

    int line_count = (int)(sizeof(lines) / sizeof(lines[0]));
    int hint_count = 2;

    if (max_width == -1) {
        max_width = 0;
        for (int i = 0; i < line_count; i++) {
            int width = strUTF8Width(lines[i]);
            widths[i] = width;
            if (width > max_width) {
                max_width = width;
            }
        }
    }

    // Clear the surface
    const ScreenStyle bg_style = {
        .fg = gEditor.color_cfg[UI_COLOR_HL_NORMAL],
        .bg = gEditor.color_cfg[UI_COLOR_BG],
    };

    for (int i = 0; i < s.h; i++) {
        ScreenCell* row = SURFACE_ROW(s, i);
        screenClearCells(row, s.w, 0, s.w, bg_style);
    }

    if (!intro.int_value)
        return;

    if (s.h < line_count)
        return;

    if (s.w < max_width)
        return;

    ScreenStyle text_style = {
        .fg = gEditor.color_cfg[UI_COLOR_HL_NORMAL],
        .bg = gEditor.color_cfg[UI_COLOR_BG],
    };
    ScreenStyle hint_style = {
        .fg = gEditor.color_cfg[UI_COLOR_HL_COMMENT],
        .bg = gEditor.color_cfg[UI_COLOR_BG],
    };

    int top = 1;  // top status bar
    int start_row = top + (s.h - line_count) / 2;
    if (start_row + line_count > s.h)
        return;

    for (int i = 0; i < line_count; i++) {
        ScreenCell* row = SURFACE_ROW(s, start_row + i);
        const char* line = lines[i];
        int width = widths[i];
        int start_col = (s.w - width) / 2;

        ScreenStyle style;
        if (i < line_count - hint_count) {
            style = text_style;
        } else {
            style = hint_style;
        }

        screenPutUtf8(row, s.w, start_col, line, style);
    }
}

static bool getCursor(Panel* self, UICursor* out) {
    UNUSED(self);
    UNUSED(out);
    return false;
}

static void onFocus(Panel* self, bool focused) {
    UNUSED(self);
    if (focused) {
        editorHelpSetMsg(HELP_GLOBAL);
    }
}

static void keyEvent(Panel* self, KeyEvent event) {
    UNUSED(self);
    UNUSED(event);
}

static bool mouseEvent(Panel* self, UIMouseEvent mouse_event) {
    UNUSED(self);
    UNUSED(mouse_event.state);
    return false;
}
