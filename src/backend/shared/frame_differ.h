#ifndef FRAME_DIFFER_H
#define FRAME_DIFFER_H

#include "ui/surface.h"

typedef struct FrameDiffer {
    bool screen_size_updated;
    Surface screen;
    Surface old_screen;
} FrameDiffer;

void frameDifferInit(FrameDiffer* self);
void frameDifferFree(FrameDiffer* self);

Surface frameDifferGetSurface(FrameDiffer* self, int width, int height);

typedef void (*FrameDifferDrawCallback)(void* ctx,
                                        int x,
                                        int y,
                                        const ScreenCell* cells,
                                        int length);
void frameDifferDraw(FrameDiffer* self,
                     bool force_redraw,
                     FrameDifferDrawCallback callback,
                     void* ctx);

#endif
