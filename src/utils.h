#ifndef UTILS_H
#define UTILS_H

#include "row.h"

#define ANSI_CLEAR "\x1b[m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

#define ABUF_GROWTH_RATE 1.5f
#define ABUF_INIT \
    { NULL, 0 }

typedef struct {
    char* buf;
    size_t len;
    size_t capacity;
} abuf;

typedef struct Color {
    int r, g, b;
} Color;

void* malloc_s(size_t size);
void* calloc_s(size_t n, size_t size);
void* realloc_s(void* ptr, size_t size);

void abufAppend(abuf* ab, const char* s);
void abufAppendN(abuf* ab, const char* s, size_t n);
void abufFree(abuf* ab);

int editorRowCxToRx(EditorRow* row, int cx);
int editorRowRxToCx(EditorRow* row, int rx);
int editorRowSxToCx(EditorRow* row, int sx);

Color strToColor(const char* color);
int colorToStr(Color color, char buf[8]);
int colorToANSI(Color color, char ansi[32], int is_bg);

int isSeparator(char c);

#endif
