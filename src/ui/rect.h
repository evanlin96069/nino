#ifndef UI_RECT_H
#define UI_RECT_H

typedef struct Rect {
    int x, y, w, h;
} Rect;

static inline bool rectContains(Rect rect, int x, int y) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y &&
           y < rect.y + rect.h;
}

static inline void rectToLocal(Rect rect,
                               int x,
                               int y,
                               int* local_x,
                               int* local_y) {
    *local_x = x - rect.x;
    *local_y = y - rect.y;
}

static inline void rectToGlobal(Rect rect,
                                int local_x,
                                int local_y,
                                int* x,
                                int* y) {
    *x = local_x + rect.x;
    *y = local_y + rect.y;
}

#endif
