#ifndef EDITOR_H
#define EDITOR_H

#include <sys/types.h>

#include "action.h"
#include "config.h"
#include "file_io.h"
#include "file_types.h"
#include "row.h"
#include "select.h"

#define EDITOR_FILE_MAX_SLOT 32

#ifdef _WIN32

#include <Windows.h>

extern HANDLE hStdin;
extern HANDLE hStdout;

#endif

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
#ifdef _WIN32
    BY_HANDLE_FILE_INFORMATION file_info;
#else
    ino_t file_inode;
#endif

    // Text buffers
    EditorRow* row;

    // Syntax highlight information
    const EditorSyntax* syntax;

    // Undo redo
    EditorActionList* action_head;
    EditorActionList* action_current;
} EditorFile;

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

    // Prompt message (left/right)
    char status_msg[2][64];

    // Files
    EditorFile files[EDITOR_FILE_MAX_SLOT];
    int file_count;
    int file_index;
    int tab_offset;
    int tab_displayed;

    // File explorer
    bool explorer_focus;
    int explorer_prefer_width;
    int explorer_width;
    int explorer_offset;
    int explorer_last_line;  // Last displayed line
    int explorer_select;
    EditorExplorerNode* explorer_node;
} Editor;

// Text editor
extern Editor gEditor;

// Current file
extern EditorFile* gCurFile;

void editorInit();
void editorFree();
void editorFreeFile(EditorFile* file);

// Multiple files control
int editorAddFile(EditorFile* file);
void editorRemoveFile(int index);
void editorChangeToFile(int index);

#endif
