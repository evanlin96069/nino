#include "terminal.h"

#include "editor/editor.h"

#include "utils/unicode.h"
#include "utils/utils.h"

#include "ui/compositor.h"
#include "ui/surface.h"

#include "backend/shared/frame_differ.h"

#include "os.h"

static inline void appendColorParams(Str* s, Color color, bool is_bg) {
    switch (color.kind) {
        case COLOR_DEFAULT:
            strAppend(s, svFromCStr(is_bg ? "49" : "39"));
            break;

        case COLOR_ANSI16: {
            int code = str_color_map[color.index].value;
            if (is_bg)
                code += 10;
            char buf[16];
            int len = snprintf(buf, sizeof(buf), "%d", code);
            strAppendN(s, buf, len);
        } break;

        case COLOR_256: {
            char buf[16];
            int len = snprintf(buf, sizeof(buf), "%d;5;%d", is_bg ? 48 : 38,
                               color.index);
            strAppendN(s, buf, len);
        } break;

        case COLOR_RGB: {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "%d;2;%d;%d;%d",
                               is_bg ? 48 : 38, color.r, color.g, color.b);
            strAppendN(s, buf, len);
        } break;

        default:
            break;
    }
}

static void setColor(Str* s, Color color, bool is_bg) {
    strAppend(s, svFromCStr("\x1b["));
    appendColorParams(s, color, is_bg);
    strPush(s, 'm');
}

static void setColors(Str* s, Color fg, Color bg) {
    strAppend(s, svFromCStr("\x1b["));
    appendColorParams(s, fg, false);
    strPush(s, ';');
    appendColorParams(s, bg, true);
    strPush(s, 'm');
}

static void gotoXY(Str* s, int x, int y) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", x, y);

    strAppendN(s, buf, len);
}

static void updateStyle(Str* s,
                        const ScreenStyle* old_style,
                        const ScreenStyle* new_style) {
    if (!new_style)
        return;

    bool update_fg = (!old_style || !colorEql(old_style->fg, new_style->fg));
    bool update_bg = (!old_style || !colorEql(old_style->bg, new_style->bg));
    if (update_fg && update_bg) {
        setColors(s, new_style->fg, new_style->bg);
    } else if (update_fg) {
        setColor(s, new_style->fg, false);
    } else if (update_bg) {
        setColor(s, new_style->bg, true);
    }
}

static void drawCallback(void* ctx,
                         int x,
                         int y,
                         const ScreenCell* cells,
                         int length) {
    if (!cells || length <= 0)
        return;

    Str* s = ctx;

    const ScreenStyle* old_style = NULL;

    gotoXY(s, y + 1, x + 1);

    for (int i = 0; i < length; i++) {
        const ScreenCell* cell = &cells[i];
        Grapheme grapheme = cell->grapheme;

        updateStyle(s, old_style, &cell->style);
        old_style = &cell->style;

        if (cell->continuation || grapheme.size == 0 || grapheme.width == 0) {
            // These are not supposed to happen
            // Default to white space
            strPush(s, ' ');
            continue;
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
            if (i + offset >= length || !cells[i + offset].continuation) {
                canDraw = false;
                break;
            }
            offset++;
        }

        i += offset - 1;

        if (!canDraw) {
            // Draw spaces until filling the character width we can draw
            for (int j = 0; j < offset; j++) {
                strPush(s, ' ');
            }
        } else {
            strAppendN(s, output, (size_t)utf8_len);
            for (int j = 1; j < grapheme.size; i++) {
                utf8_len = encodeUTF8(grapheme.cluster[j], output);
                if (utf8_len != -1) {
                    strAppendN(s, output, (size_t)utf8_len);
                }
            }
        }
    }
}

static FrameDiffer frame_differ;
static Str render_buffer;

void terminalInit(void) {
    frameDifferInit(&frame_differ);
}

void terminalFree(void) {
    frameDifferFree(&frame_differ);
    strFree(&render_buffer);
}

static void startDrawing(Str* s) {
    strClear(s);
    strAppend(s, svFromCStr(ANSI_SYNC_BEGIN ANSI_CURSOR_HIDE));
}

static void finishDrawing(Str* s) {
    strAppend(s, svFromCStr(ANSI_CLEAR_STYLE ANSI_SYNC_END));
    writeConsoleAll(s->data, s->size);
}

static void drawCursor(Str* s, UICursor cursor) {
    if (cursor.visible) {
        gotoXY(s, cursor.y + 1, cursor.x + 1);
        strAppend(s, svFromCStr(ANSI_CURSOR_SHOW));
    } else {
        strAppend(s, svFromCStr(ANSI_CURSOR_HIDE));
    }
}

void terminalRefreshScreen(bool force_redraw) {
    Surface s = frameDifferGetSurface(&frame_differ, gEditor.screen_width,
                                      gEditor.screen_height);

    UICursor cursor;
    editorDrawScreen(s, &cursor);

    startDrawing(&render_buffer);

    frameDifferDraw(&frame_differ, force_redraw, drawCallback, &render_buffer);
    drawCursor(&render_buffer, cursor);

    finishDrawing(&render_buffer);
}
