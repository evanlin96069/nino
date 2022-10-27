#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>
#include "row.h"
#include "config.h"
#include "file_types.h"

typedef struct Editor {
    int cx, cy;
    int rx;
    int sx;
    int px;
    int row_offset;
    int col_offset;
    int rows;
    int cols;
    int num_rows;
    int num_rows_digits;
    EditorRow* row;
    int state;
    int dirty;
    int is_selected;
    int select_x, select_y;
    int bracket_autocomplete;
    char* filename;
    char status_msg[80];
    struct termios orig_termios;
    EditorSyntax* syntax;
    EditorConfig* cfg;
} Editor;

extern Editor E;

void editorInit();
void editorInsertChar(int c);
void editorInsertNewline();
void editorDelChar();

#endif
