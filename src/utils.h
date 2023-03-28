#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

#include "row.h"

#define ANSI_CLEAR "\x1b[m"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_NOT_UNDERLINE "\x1b[24m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_NOT_INVERT "\x1b[27m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

#define ABUF_GROWTH_RATE 1.5f
#define ABUF_INIT \
    { NULL, 0, 0 }

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

int editorRowPreviousUTF8(EditorRow* row, int cx);
int editorRowNextUTF8(EditorRow* row, int cx);

int editorRowCxToRx(const EditorRow* row, int cx);
int editorRowRxToCx(const EditorRow* row, int rx);

Color strToColor(const char* color);
int colorToStr(Color color, char buf[8]);
int colorToANSI(Color color, char ansi[32], int is_bg);

typedef int (*IsCharFunc)(int c);
int isSeparator(int c);
int isNonSeparator(int c);
int isNonIdentifierChar(int c);
int isIdentifierChar(int c);
int isNonSpace(int c);

int getDigit(int n);

#endif
