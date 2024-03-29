#ifndef ROW_H
#define ROW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct EditorFile;
typedef struct EditorFile EditorFile;

typedef struct EditorRow {
    int size;
    int rsize;
    char* data;
    uint8_t* hl;
    int hl_open_comment;
} EditorRow;

void editorUpdateRow(EditorFile* file, EditorRow* row);
void editorInsertRow(EditorFile* file, int at, const char* s, size_t len);
void editorFreeRow(EditorRow* row);
void editorDelRow(EditorFile* file, int at);
void editorRowInsertChar(EditorFile* file, EditorRow* row, int at, int c);
void editorRowDelChar(EditorFile* file, EditorRow* row, int at);
void editorRowAppendString(EditorFile* file, EditorRow* row, const char* s,
                           size_t len);

// On gCurFile
void editorInsertChar(int c);
void editorInsertUnicode(uint32_t unicode);
void editorInsertNewline(void);
void editorDelChar(void);

// UTF-8
int editorRowPreviousUTF8(EditorRow* row, int cx);
int editorRowNextUTF8(EditorRow* row, int cx);

// Cx Rx
int editorRowCxToRx(const EditorRow* row, int cx);
int editorRowRxToCx(const EditorRow* row, int rx);

#endif
