#include "frame_differ.h"

void frameDifferInit(FrameDiffer* self) {
    self->screen_size_updated = false;
    surfaceInit(&self->screen, 0, 0);
    surfaceInit(&self->old_screen, 0, 0);
}

void frameDifferFree(FrameDiffer* self) {
    surfaceFree(&self->screen);
    surfaceFree(&self->old_screen);
}

Surface frameDifferGetSurface(FrameDiffer* self, int width, int height) {
    if (width <= 0 || height <= 0) {
        return (Surface){0};
    }

    if (width != self->screen.w || height != self->screen.h) {
        surfaceFree(&self->screen);
        surfaceFree(&self->old_screen);
        surfaceInit(&self->screen, width, height);
        surfaceInit(&self->old_screen, width, height);
        self->screen_size_updated = true;
    } else {
        Surface temp = self->old_screen;
        self->old_screen = self->screen;
        self->screen = temp;
    }

    return self->screen;
}

static bool diffRow(ScreenCell* row,
                    ScreenCell* old_row,
                    int length,
                    int* start_col,
                    int* end_col) {
    int start = -1;
    int end = -1;

    for (int i = 0; i < length; i++) {
        if (!cellEql(&row[i], &old_row[i])) {
            start = i;
            break;
        }
    }
    for (int i = length - 1; i > start; i--) {
        ScreenCell* c = &row[i];
        if (!cellEql(c, &old_row[i])) {
            // Don't split a grapheme
            int offset = 1;
            while (i + offset < length && offset < (int)c->grapheme.width) {
                if (!row[i + offset].continuation)
                    break;
                offset++;
            }

            end = i + offset;
            if (end >= length - 1)
                end = length - 1;

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

void frameDifferDraw(FrameDiffer* self,
                     bool force_redraw,
                     FrameDifferDrawCallback callback,
                     void* ctx) {
    force_redraw |= self->screen_size_updated;
    self->screen_size_updated = false;

    if (!callback)
        return;

    Surface screen = self->screen;
    Surface old_screen = self->old_screen;

    for (int i = 0; i < screen.h; i++) {
        ScreenCell* row = &SURFACE_AT(screen, 0, i);
        ScreenCell* old_row = &SURFACE_AT(old_screen, 0, i);

        if (force_redraw) {
            callback(ctx, 0, i, row, screen.w);
        } else {
            int start_col, end_col;
            if (diffRow(row, old_row, screen.w, &start_col, &end_col)) {
                callback(ctx, start_col, i, &row[start_col],
                         end_col - start_col + 1);
            }
        }
    }
}
