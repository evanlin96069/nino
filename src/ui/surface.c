#include "ui/surface.h"

#include "unicode.h"

void surfaceInit(Surface* s, int w, int h) {
    s->w = w;
    s->h = h;
    s->stride = w;
    s->cells = malloc_s(w * h * sizeof(ScreenCell));
}

void surfaceFree(Surface* s) {
    s->w = 0;
    s->h = 0;
    s->stride = 0;
    free(s->cells);
    s->cells = NULL;
}

void surfaceResize(Surface* s, int w, int h) {
    s->w = w;
    s->h = h;
    s->stride = w;
    free(s->cells);
    s->cells = malloc_s(w * h * sizeof(ScreenCell));
}

static const Grapheme grapheme_space = {
    .cluster = {[0] = ' '},
    .size = 1,
    .width = 1,
};

void screenClearCells(ScreenCell* row,
                      int max_width,
                      int x,
                      int count,
                      ScreenStyle style) {
    if (x >= max_width)
        return;

    int to_clear = count;
    if (x + count > max_width) {
        to_clear = max_width - x;
    }

    for (int i = 0; i < to_clear; i++) {
        row[x + i].continuation = false;
        row[x + i].grapheme = grapheme_space;
        row[x + i].style = style;
    }
}

// Put a grapheme in a cell
// Marks the cells occupied by the grapheme as continuation
// Returns the number of cells used (grapheme width)
int screenPutGrapheme(ScreenCell* row,
                      int max_width,
                      int x,
                      const Grapheme* grapheme,
                      const ScreenStyle* style) {
    if (x >= max_width || grapheme->width <= 0)
        return 0;

    int width = grapheme->width;
    if (x + width > max_width)
        width = max_width - x;

    // Set the first cell
    row[x].continuation = false;
    row[x].grapheme = *grapheme;
    row[x].style = *style;

    // Mark continuation cells
    for (int i = 1; i < width; i++) {
        row[x + i].continuation = true;
        row[x + i].style = *style;
    }

    return width;
}

int screenPutChar(ScreenCell* row,
                  int max_width,
                  int x,
                  uint32_t code_point,
                  const ScreenStyle* style) {
    int width = unicodeWidth(code_point);
    if (width < 0 || x >= max_width)
        return 0;

    Grapheme grapheme = {0};
    grapheme.cluster[0] = code_point;
    grapheme.size = 1;
    grapheme.width = width;

    return screenPutGrapheme(row, max_width, x, &grapheme, style);
}

int screenPutUtf8(ScreenCell* row,
                  int max_width,
                  int x,
                  const char* s,
                  const ScreenStyle style) {
    if (!s)
        return 0;

    int start_x = x;
    size_t len = strlen(s);
    const char* p = s;

    while (*p != '\0' && x < max_width) {
        // Skip zero-width characters until we find a base character (width > 0)
        size_t byte_size;
        uint32_t code_point = decodeUTF8(p, len, &byte_size);

        if (byte_size == 0)
            break;

        int width = unicodeWidth(code_point);
        if (width <= 0) {
            // Invalid or zero-width character
            p += byte_size;
            len -= byte_size;
            continue;
        }

        if (x + width > max_width)
            break;

        // Build grapheme cluster
        Grapheme grapheme = {0};
        grapheme.cluster[0] = code_point;
        grapheme.size = 1;
        grapheme.width = width;

        p += byte_size;
        len -= byte_size;

        // Add following zero-width characters
        while (grapheme.size < MAX_CLUSTER_SIZE && *p != '\0' && len > 0) {
            size_t comb_byte_size;
            uint32_t comb_code_point = decodeUTF8(p, len, &comb_byte_size);

            if (comb_byte_size == 0)
                break;

            int comb_width = unicodeWidth(comb_code_point);
            if (comb_width < 0) {
                // Invalid character
                p += comb_byte_size;
                len -= comb_byte_size;
                break;
            }

            if (comb_width > 0)
                break;

            grapheme.cluster[grapheme.size] = comb_code_point;
            grapheme.size++;

            p += comb_byte_size;
            len -= comb_byte_size;
        }

        x += screenPutGrapheme(row, max_width, x, &grapheme, &style);
    }

    return x - start_x;
}

int screenPutAscii(ScreenCell* row,
                   int max_width,
                   int x,
                   const char* s,
                   const ScreenStyle style) {
    if (!s)
        return 0;

    int start_x = x;
    const char* p = s;

    while (*p != '\0' && x < max_width) {
        unsigned char c = (unsigned char)*p;

        // Fast path: ASCII characters are always width 1
        Grapheme grapheme = {0};
        grapheme.cluster[0] = c;
        grapheme.size = 1;
        grapheme.width = 1;

        x += screenPutGrapheme(row, max_width, x, &grapheme, &style);
        p++;
    }

    return x - start_x;
}
