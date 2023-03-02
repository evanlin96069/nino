#ifndef ROW_H
#define ROW_H

#include <stdbool.h>
#include <stddef.h>

typedef struct EditorRow {
    int idx;
    int size;
    int rsize;
    char* data;
    unsigned char* hl;
    int hl_open_comment;
} EditorRow;

void editorUpdateRow(EditorRow* row);
void editorInsertRow(int at, const char* s, size_t len);
void editorFreeRow(EditorRow* row);
void editorDelRow(int at);
void editorRowInsertChar(EditorRow* row, int at, int c);
void editorRowDelChar(EditorRow* row, int at);
void editorRowAppendString(EditorRow* row, const char* s, size_t len);

#endif
