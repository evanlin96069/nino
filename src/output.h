#ifndef OUTPUT_H
#define OUTPUT_H

#include "utils.h"

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

void editorRefreshScreen(void);

#endif
