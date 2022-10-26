#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>
#include "file_types.h"

typedef struct EditorRow {
    int idx;
    int size;
    int rsize;
    char* data;
    char* render;
    unsigned char* hl;
    unsigned char* selected;
    int hl_open_comment;
} EditorRow;

typedef struct EditorConfig {
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
    EditorSyntax* syntax;
    struct termios orig_termios;
} EditorConfig;

extern EditorConfig E;

void initEditor();
void editorInsertChar(int c);
void editorInsertNewline();
void editorDelChar();

#endif
