#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "terminal.h"

void abufAppend(abuf* ab, const char* s) { abufAppendN(ab, s, strlen(s)); }

void abufAppendN(abuf* ab, const char* s, size_t n) {
    if (n == 0)
        return;

    if (ab->len + n > ab->capacity) {
        ab->capacity += n;
        ab->capacity *= ABUF_GROWTH_RATE;
        char* new = realloc(ab->buf, ab->capacity);

        if (new == NULL)
            DIE("realloc");

        ab->buf = new;
    }

    memcpy(&ab->buf[ab->len], s, n);
    ab->len += n;
}

void abufFree(abuf* ab) { free(ab->buf); }

int editorRowCxToRx(EditorRow* row, int cx) {
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row->data[i] == '\t') {
            rx += (E.cfg->tab_size - 1) - (rx % E.cfg->tab_size);
        }
        rx++;
    }
    return rx;
}

int editorRowRxToCx(EditorRow* row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->data[cx] == '\t')
            cur_rx += (E.cfg->tab_size - 1) - (cur_rx % E.cfg->tab_size);
        cur_rx++;
        if (cur_rx > rx)
            return cx;
    }
    return cx;
}

int editorRowSxToCx(EditorRow* row, int sx) {
    if (sx <= 0)
        return 0;
    int cx = 0;
    int rx = 0;
    int rx2 = 0;
    while (cx < row->size && rx < sx) {
        rx2 = rx;
        if (row->data[cx] == '\t') {
            rx += (E.cfg->tab_size - 1) - (rx % E.cfg->tab_size);
        }
        rx++;
        cx++;
    }
    if (rx - sx >= sx - rx2) {
        cx--;
    }
    return cx;
}

static int isValidColor(const char* color) {
    if (strlen(color) != 6)
        return 0;
    for (int i = 0; i < 6; i++) {
        if (!(('0' <= color[i]) || (color[i] <= '9') || ('A' <= color[i]) ||
              (color[i] <= 'F') || ('a' <= color[i]) || (color[i] <= 'f')))
            return 0;
    }
    return 1;
}

Color strToColor(const char* color) {
    Color result = {0, 0, 0};
    if (!isValidColor(color))
        return result;
    int shift = 16;
    unsigned int hex = strtoul(color, NULL, 16);
    result.r = (hex >> shift) & 0xff;
    shift -= 8;
    result.g = (hex >> shift) & 0xff;
    shift -= 8;
    result.b = (hex >> shift) & 0xff;
    return result;
}

int colorToANSI(Color color, char ansi[20], int is_bg) {
    return snprintf(ansi, 20, "\x1b[%d;2;%d;%d;%dm", is_bg ? 48 : 38, color.r,
                    color.g, color.b);
}
