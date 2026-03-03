#ifndef ROW_H
#define ROW_H

#include "utils.h"

typedef struct EditorFile EditorFile;

typedef struct EditorRow {
    int size;
    int rsize;
    char* data;
    size_t capacity;
    uint8_t* hl;
    int hl_open_comment;
} EditorRow;

void editorRowEnsureCapacity(EditorRow* row, size_t size);
void editorUpdateRow(EditorFile* file, EditorRow* row);
void editorInsertRow(EditorFile* file, int at, const char* s, size_t len);
void editorFreeRow(EditorRow* row);
void editorDelRow(EditorFile* file, int at);
void editorRowInsertChar(EditorFile* file, EditorRow* row, int at, int c);
void editorRowDelChar(EditorFile* file, EditorRow* row, int at);
void editorRowDeleteRange(EditorFile* file, EditorRow* row, int from, int to);
void editorRowAppendString(EditorFile* file,
                           EditorRow* row,
                           const char* s,
                           size_t len);
void editorRowInsertString(EditorFile* file,
                           EditorRow* row,
                           int at,
                           const char* s,
                           size_t len);

// UTF-8
int editorRowPreviousUTF8(const EditorRow* row, int cx);
int editorRowNextUTF8(const EditorRow* row, int cx);

// Cx Rx
int editorRowCxToRx(const EditorRow* row, int cx);
int editorRowRxToCx(const EditorRow* row, int rx);

// Word movement
int editorRowNextCharIndex(const EditorRow* row, int index, IsCharFunc is_char);
int editorRowPrevCharIndex(const EditorRow* row, int index, IsCharFunc is_char);
int editorRowWordRight(const EditorRow* row, int cx);
int editorRowWordLeft(const EditorRow* row, int cx);
void editorRowSelectWord(const EditorRow* row,
                         int cx,
                         IsCharFunc is_char,
                         int* select_start,
                         int* select_end);

#endif
