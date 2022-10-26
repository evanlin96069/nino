#ifndef ROW_H
#define ROW_H

#include <stddef.h>
#include "editor.h"

void editorUpdateRow(EditorRow* row);
void editorInsertRow(int at, char* s, size_t len);
void editorFreeRow(EditorRow* row);
void editorDelRow(int at);
void editorRowInsertChar(EditorRow* row, int at, int c);
void editorRowDelChar(EditorRow* row, int at);
void editorRowAppendString(EditorRow* row, char* s, size_t len);

#endif
