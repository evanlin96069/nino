#ifndef UTILS_H
#define UTILS_H

#include "editor.h"

#define ABUF_INIT {NULL, 0}

typedef struct {
    char* buf;
    int len;
} abuf;

void abufAppend(abuf* ab, const char* s, int len);
void abufFree(abuf* ab);

int editorRowCxToRx(EditorRow* row, int cx);
int editorRowRxToCx(EditorRow* row, int rx);
int editorRowSxToCx(EditorRow* row, int sx);

#endif
