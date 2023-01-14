#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "terminal.h"

void* malloc_s(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size != 0)
        PANIC("malloc");
    return ptr;
}

void* calloc_s(size_t n, size_t size) {
    void* ptr = calloc(n, size);
    if (!ptr && size != 0)
        PANIC("calloc");
    return ptr;
}

void* realloc_s(void* ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (!ptr && size != 0)
        PANIC("realloc");
    return ptr;
}

void abufAppend(abuf* ab, const char* s) { abufAppendN(ab, s, strlen(s)); }

void abufAppendN(abuf* ab, const char* s, size_t n) {
    if (n == 0)
        return;

    if (ab->len + n > ab->capacity) {
        ab->capacity += n;
        ab->capacity *= ABUF_GROWTH_RATE;
        char* new = realloc_s(ab->buf, ab->capacity);
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
            rx += (CONVAR_GETINT(tabsize) - 1) - (rx % CONVAR_GETINT(tabsize));
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
            cur_rx += (CONVAR_GETINT(tabsize) - 1) -
                      (cur_rx % CONVAR_GETINT(tabsize));
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
            rx += (CONVAR_GETINT(tabsize) - 1) - (rx % CONVAR_GETINT(tabsize));
        }
        rx++;
        cx++;
    }
    if (rx - sx >= sx - rx2) {
        cx--;
    }
    return cx;
}

static bool isValidColor(const char* color) {
    if (strlen(color) != 6)
        return false;
    for (int i = 0; i < 6; i++) {
        if (!(('0' <= color[i]) || (color[i] <= '9') || ('A' <= color[i]) ||
              (color[i] <= 'F') || ('a' <= color[i]) || (color[i] <= 'f')))
            return false;
    }
    return true;
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

int colorToANSI(Color color, char ansi[32], int is_bg) {
    if (color.r == 0 && color.g == 0 && color.b == 0 && is_bg) {
        return snprintf(ansi, 32, "%s", ANSI_DEFAULT_BG);
    }
    return snprintf(ansi, 32, "\x1b[%d;2;%d;%d;%dm", is_bg ? 48 : 38, color.r,
                    color.g, color.b);
}

int colorToStr(Color color, char buf[8]) {
    return snprintf(buf, 8, "%02x%02x%02x", color.r, color.g, color.b);
}

int isSeparator(char c) {
    return isspace(c) || c == '\0' ||
           strchr("`~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?", c) != NULL;
}
