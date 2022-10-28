#ifndef UTILS_H
#define UTILS_H

#include "row.h"

#define ANSI_CLEAR "\x1b[m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

#define ABUF_INIT \
    { NULL, 0 }

typedef struct {
    char* buf;
    int len;
} abuf;

typedef struct Color {
    int r, g, b;
} Color;

void abufAppend(abuf* ab, const char* s);
void abufAppendN(abuf* ab, const char* s, size_t n);
void abufFree(abuf* ab);

int editorRowCxToRx(EditorRow* row, int cx);
int editorRowRxToCx(EditorRow* row, int rx);
int editorRowSxToCx(EditorRow* row, int sx);

Color strToColor(const char* color);
int colorToANSI(Color color, char ansi[20], int is_bg);

#endif
