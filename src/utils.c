#include "utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defines.h"
#include "editor.h"
#include "terminal.h"
#include "unicode.h"

void *malloc_s(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size != 0)
        PANIC("malloc");
    return ptr;
}

void *calloc_s(size_t n, size_t size) {
    void *ptr = calloc(n, size);
    if (!ptr && size != 0)
        PANIC("calloc");
    return ptr;
}

void *realloc_s(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (!ptr && size != 0)
        PANIC("realloc");
    return ptr;
}

void arenaInit(Arena* arena, size_t capacity) {
    arena->capacity = capacity;
    arena->size = 0;
    arena->data = malloc_s(capacity);
}

void* arenaAlloc(Arena* arena, size_t size) {
    if (arena->size + size > arena->capacity) {
        PANIC("arenaAlloc");
    }

    void* result = arena->data + arena->size;
    arena->size += size;
    return result;
}

void arenaReset(Arena* arena) { arena->size = 0; }

void arenaDeinit(Arena* arena) {
    arena->size = 0;
    arena->capacity = 0;
    free(arena->data);
    arena->data = NULL;
}

int osRead(char *buf, int n) {
#ifdef _WIN32
    DWORD nread = 0;
    ReadConsoleA(hStdin, buf, n, &nread, NULL);
    return (int)nread;
#else
    return read(STDIN_FILENO, buf, n);
#endif
}

void abufAppend(abuf *ab, const char *s) { abufAppendN(ab, s, strlen(s)); }

void abufAppendN(abuf *ab, const char *s, size_t n) {
    if (n == 0)
        return;

    if (ab->len + n > ab->capacity) {
        ab->capacity += n;
        ab->capacity *= ABUF_GROWTH_RATE;
        char *new = realloc_s(ab->buf, ab->capacity);
        ab->buf = new;
    }

    memcpy(&ab->buf[ab->len], s, n);
    ab->len += n;
}

void abufFree(abuf *ab) { free(ab->buf); }

int editorRowNextUTF8(EditorRow *row, int cx) {
    if (cx < 0)
        return 0;

    if (cx >= row->size)
        return row->size;

    const char *s = &row->data[cx];
    size_t byte_size;
    decodeUTF8(s, row->size - cx, &byte_size);
    return cx + byte_size;
}

int editorRowPreviousUTF8(EditorRow *row, int cx) {
    if (cx <= 0)
        return 0;

    if (cx > row->size)
        return row->size;

    int i = 0;
    size_t byte_size;
    while (i < cx) {
        decodeUTF8(&row->data[i], row->size - i, &byte_size);
        i += byte_size;
    }
    return i - byte_size;
}

int editorRowCxToRx(const EditorRow *row, int cx) {
    int rx = 0;
    int i = 0;
    while (i < cx) {
        size_t byte_size;
        uint32_t unicode = decodeUTF8(&row->data[i], row->size - i, &byte_size);
        if (unicode == '\t') {
            rx += (CONVAR_GETINT(tabsize) - 1) - (rx % CONVAR_GETINT(tabsize)) +
                  1;
        } else {
            int width = unicodeWidth(unicode);
            if (width < 0)
                width = 1;
            rx += width;
        }
        i += byte_size;
    }
    return rx;
}

int editorRowRxToCx(const EditorRow *row, int rx) {
    int cur_rx = 0;
    int cx = 0;
    while (cx < row->size) {
        size_t byte_size;
        uint32_t unicode =
            decodeUTF8(&row->data[cx], row->size - cx, &byte_size);
        if (unicode == '\t') {
            cur_rx += (CONVAR_GETINT(tabsize) - 1) -
                      (cur_rx % CONVAR_GETINT(tabsize)) + 1;
        } else {
            int width = unicodeWidth(unicode);
            if (width < 0)
                width = 1;
            cur_rx += width;
        }
        if (cur_rx > rx)
            return cx;
        cx += byte_size;
    }
    return cx;
}

static bool isValidColor(const char *color) {
    if (strlen(color) != 6)
        return false;
    for (int i = 0; i < 6; i++) {
        if (!(('0' <= color[i]) || (color[i] <= '9') || ('A' <= color[i]) ||
              (color[i] <= 'F') || ('a' <= color[i]) || (color[i] <= 'f')))
            return false;
    }
    return true;
}

Color strToColor(const char *color) {
    Color result = {0, 0, 0};
    if (!isValidColor(color))
        return result;

    int shift = 16;
    unsigned int hex = strtoul(color, NULL, 16);
    result.r = (hex >> shift) & 0xFF;
    shift -= 8;
    result.g = (hex >> shift) & 0xFF;
    shift -= 8;
    result.b = (hex >> shift) & 0xFF;
    return result;
}

void setColor(abuf *ab, Color color, int is_bg) {
    char buf[32];
    int len;
    if (color.r == 0 && color.g == 0 && color.b == 0 && is_bg) {
        len = snprintf(buf, sizeof(buf), "%s", ANSI_DEFAULT_BG);
    } else {
        len = snprintf(buf, sizeof(buf), "\x1b[%d;2;%d;%d;%dm", is_bg ? 48 : 38,
                       color.r, color.g, color.b);
    }
    abufAppendN(ab, buf, len);
}

void gotoXY(abuf *ab, int x, int y) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", x, y);
    abufAppendN(ab, buf, len);
}

int colorToStr(Color color, char buf[8]) {
    return snprintf(buf, 8, "%02x%02x%02x", color.r, color.g, color.b);
}

int isSeparator(int c) {
    return strchr("`~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?", c) != NULL;
}

int isNonSeparator(int c) { return !isSeparator(c); }

int isNonIdentifierChar(int c) {
    return isspace(c) || c == '\0' || isSeparator(c);
}

int isIdentifierChar(int c) { return !isNonIdentifierChar(c); }

int isNonSpace(int c) { return !isspace(c); }

int getDigit(int n) {
    if (n < 10)
        return 1;
    if (n < 100)
        return 2;
    if (n < 1000)
        return 3;
    if (n < 10000000) {
        if (n < 1000000) {
            if (n < 10000)
                return 4;
            return 5 + (n >= 100000);
        }
        return 7;
    }
    if (n < 1000000000)
        return 8 + (n >= 100000000);
    return 10;
}

#ifdef _WIN32
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

const char *getBaseName(const char *path) {
    const char *name = strrchr(path, PATH_SEPARATOR);
    return name ? name + 1 : path;
}

char *getDirName(char *path) {
    char *name = strrchr(path, PATH_SEPARATOR);
    if (!name) {
        name = path;
        *name = '.';
        name++;
    }
    *name = '\0';
    return path;
}
