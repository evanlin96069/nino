#include "ui/compositor.h"

#include "ui/layout.h"
#include "ui/surface.h"

void uiFree(UI* ui) {
    layoutFree(ui->root);
    ui->root = NULL;
    ui->focused_panel = NULL;
    vector_free(ui->separators);
}

void uiComposite(UI* ui, Surface s, ScreenStyle sep_style) {
    if (s.w <= 0 || s.h <= 0)
        return;

    Rect available = {
        .x = 0,
        .y = 0,
        .w = s.w,
        .h = s.h,
    };
    vector_clear(ui->separators);
    layoutCompute(ui->root, available, &ui->separators);
    layoutRender(ui->root, s);

    // Draw separators
    ScreenCell sep_cell = {
        .continuation = false,
        .grapheme =
            {
                .cluster = {0},
                .size = 1,
                .width = 1,
            },
        .style = sep_style,
    };

    for (uint32_t i = 0; i < ui->separators.size; i++) {
        const Separator* sep = &ui->separators.data[i];
        bool leftright = (sep->parent->kind == LAYOUT_LEFTRIGHT);
        if (leftright) {
            sep_cell.grapheme.cluster[0] = '|';
        } else {
            sep_cell.grapheme.cluster[0] = '-';
        }

        for (int x = sep->rect.x; x < sep->rect.x + sep->rect.w; x++) {
            for (int y = sep->rect.y; y < sep->rect.y + sep->rect.h; y++) {
                SURFACE_AT(s, x, y) = sep_cell;
            }
        }
    }
}

void uiProcessInput(UI* ui, EditorInput input) {}
