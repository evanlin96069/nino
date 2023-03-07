#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>

#include "action.h"
#include "config.h"
#include "file_types.h"
#include "row.h"
#include "select.h"

#define MAX_EDITOR_FILE_SLOT 10

typedef struct Editor {
    // Raw screen size
    int screen_rows;
    int screen_cols;

    // Text field size
    int display_rows;

    // Editor mode
    bool loading;
    int state;
    bool mouse_mode;

    // Cursor position for prompt
    int px;

    // Copy paste
    EditorClipboard clipboard;

    // Color settings
    EditorColorScheme color_cfg;

    // ConCmd linked list
    EditorConCmd* cvars;

    // For restore termios when exit
    struct termios orig_termios;

    // Button status message (left/right)
    char status_msg[2][64];
} Editor;

typedef struct EditorFile {
    // Cursor position
    EditorCursor cursor;

    // Hidden cursor x position
    int sx;

    // bracket complete level
    int bracket_autocomplete;

    // Editor offsets
    int row_offset;
    int col_offset;

    // Total line number
    int num_rows;
    int num_rows_digits;

    // File info
    int dirty;
    char* filename;

    // Text buffers
    EditorRow* row;

    // Syntax highlight information
    const EditorSyntax* syntax;

    // Undo redo
    EditorActionList action_head;
    EditorActionList* action_current;
} EditorFile;

// Text editor
extern Editor gEditor;

// Current file
extern EditorFile* gCurFile;

void editorInit();
void editorFree();
void editorInsertChar(int c);
void editorInsertNewline();
void editorDelChar();

#endif
