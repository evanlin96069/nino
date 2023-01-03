#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>

#include "action.h"
#include "config.h"
#include "file_types.h"
#include "row.h"
#include "select.h"

typedef struct Editor {
    EditorCursor cursor;

    int rx;
    int sx;
    int px;

    int row_offset;
    int col_offset;
    int screen_rows;
    int screen_cols;
    int rows;
    int cols;
    int num_rows;
    int num_rows_digits;

    int state;
    int mouse_mode;

    int dirty;
    int bracket_autocomplete;

    char* filename;
    char status_msg[80];
    struct termios orig_termios;

    EditorClipboard clipboard;
    EditorRow* row;

    EditorSyntax* syntax;

    EditorColorScheme color_cfg;

    EditorActionList action_head;
    EditorActionList* action_current;

    EditorConCmd* cvars;
} Editor;

extern Editor E;

void editorInit();
void editorFree();
void editorInsertChar(int c);
void editorInsertNewline();
void editorDelChar();

#endif
