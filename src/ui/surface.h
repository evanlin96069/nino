#ifndef UI_SURFACE_H
#define UI_SURFACE_H

#include "utils.h"

#include "ui/rect.h"

#define MAX_CLUSTER_SIZE 8  // Good enough
typedef struct Grapheme {
    uint32_t cluster[MAX_CLUSTER_SIZE];
    uint8_t size;
    uint8_t width;
} Grapheme;

typedef struct ScreenStyle {
    Color bg;
    Color fg;
} ScreenStyle;

typedef struct ScreenCell {
    bool continuation;  // Is continuation of a previous cell
    Grapheme grapheme;
    ScreenStyle style;
} ScreenCell;

typedef struct Surface {
    ScreenCell* cells;
    int w, h;
    int stride;
} Surface;

#define SURFACE_AT(s, x, y) ((s).cells[(y) * (s).stride + (x)])

// Surface
void surfaceInit(Surface* s, int w, int h);
void surfaceFree(Surface* s);
void surfaceResize(Surface* s, int w, int h);
static inline Surface surfaceSub(Surface s, Rect rect) {
    return (Surface){
        .cells = &SURFACE_AT(s, rect.x, rect.y),
        .w = rect.w,
        .h = rect.h,
        .stride = s.stride,
    };
}

// Screen cell
void screenClearCells(ScreenCell* row,
                      int max_width,
                      int x,
                      int count,
                      ScreenStyle style);
int screenPutGrapheme(ScreenCell* row,
                      int max_width,
                      int x,
                      const Grapheme* grapheme,
                      const ScreenStyle* style);
int screenPutChar(ScreenCell* row,
                  int max_width,
                  int x,
                  uint32_t code_point,
                  const ScreenStyle* style);
int screenPutUtf8(ScreenCell* row,
                  int max_width,
                  int x,
                  const char* s,
                  const ScreenStyle style);
int screenPutAscii(ScreenCell* row,
                   int max_width,
                   int x,
                   const char* s,
                   const ScreenStyle style);

#endif
