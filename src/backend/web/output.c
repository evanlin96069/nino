#include "web.h"

#include "editor/editor.h"

#include "utils/unicode.h"
#include "utils/utils.h"

#include "ui/surface.h"

#include "backend/shared/frame_differ.h"

// clang-format off

EM_JS(void,
      webDrawGrapheme,
      (int x, int y, const char* grapheme, int r, int g, int b, int a), {
    drawGrapheme(x, y, UTF8ToString(grapheme), r, g, b, a);
})

EM_JS(void,
      webDrawBackground,
      (int x, int y, int width, int r, int g, int b, int a), {
    drawBackground(x, y, width, r, g, b, a);
})

EM_JS(void, webDrawCursor, (int visible, int x, int y), {
    drawCursor(visible, x, y);
})

// clang-format on

static void drawCallback(void* ctx,
                         int x,
                         int y,
                         const ScreenCell* cells,
                         int length) {
    UNUSED(ctx);

    if (!cells || length <= 0)
        return;

    // Draw bg
    ColorRGBA curr_color = colorToRGBA(cells[0].style.bg);
    int curr_start_i = 0;
    for (int i = 1; i < length; i++) {
        ColorRGBA bg = colorToRGBA(cells[i].style.bg);
        if (bg.value != curr_color.value) {
            webDrawBackground(x + curr_start_i, y, i - curr_start_i,
                              curr_color.r, curr_color.g, curr_color.b,
                              curr_color.a);
            curr_color = bg;
            curr_start_i = i;
        }
    }
    webDrawBackground(x + curr_start_i, y, length - curr_start_i, curr_color.r,
                      curr_color.g, curr_color.b, curr_color.a);

    // Draw text
    for (int i = 0; i < length; i++) {
        ColorRGBA fg = colorToRGBA(cells[i].style.fg);
        const ScreenCell* cell = &cells[i];
        Grapheme grapheme = cell->grapheme;

        if (cell->continuation || grapheme.size == 0 || grapheme.width == 0) {
            // These are not supposed to happen
            // Leave the cells blank (background only)
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

        int start = i;
        i += offset - 1;

        // If it doesn't fit, leave the cells blank (background only)
        if (canDraw) {
            Str grapheme_str = {0};
            strAppendN(&grapheme_str, output, (size_t)utf8_len);
            for (int j = 1; j < grapheme.size; j++) {
                utf8_len = encodeUTF8(grapheme.cluster[j], output);
                if (utf8_len != -1) {
                    strAppendN(&grapheme_str, output, (size_t)utf8_len);
                }
            }
            strPush(&grapheme_str, '\0');
            webDrawGrapheme(x + start, y, grapheme_str.data, fg.r, fg.g, fg.b,
                            fg.a);
            strFree(&grapheme_str);
        }
    }
}

static FrameDiffer frame_differ;

void webInit(void) {
    frameDifferInit(&frame_differ);
}

void webFree(void) {
    frameDifferFree(&frame_differ);
}

void webRefreshScreen(bool force_redraw) {
    Surface s = frameDifferGetSurface(&frame_differ, gEditor.screen_width,
                                      gEditor.screen_height);

    UICursor cursor;
    editorDrawScreen(s, &cursor);

    frameDifferDraw(&frame_differ, force_redraw, drawCallback, NULL);
    webDrawCursor(cursor.visible, cursor.x, cursor.y);
}
