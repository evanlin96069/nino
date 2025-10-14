#ifndef EDITOR_H
#define EDITOR_H

#include "action.h"
#include "config.h"
#include "file_io.h"
#include "os.h"
#include "row.h"
#include "select.h"

#define EDITOR_FILE_MAX_SLOT 32

#define EDITOR_CON_COUNT 16
#define EDITOR_CON_LENGTH 255

#define EDITOR_PROMPT_LENGTH 255
#define EDITOR_RIGHT_PROMPT_LENGTH 32

enum EditorState {
    LOADING_MODE,
    EDIT_MODE,
    EXPLORER_MODE,
    FIND_MODE,
    GOTO_LINE_MODE,
    OPEN_FILE_MODE,
    CONFIG_MODE,
    SAVE_AS_MODE,
};

typedef struct EditorSyntax EditorSyntax;

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
    int lineno_width;

    // Encoding
    uint8_t newline;

    // File info
    char* filename;  // NULL if untitled
    int new_id;
    FileInfo file_info;

    // Text buffers
    size_t row_capacity;
    EditorRow* row;

    // Syntax highlight information
    EditorSyntax* syntax;

    // Undo redo
    int dirty;
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

    // Files
    EditorFile files[EDITOR_FILE_MAX_SLOT];
    int file_count;
    int file_index;
    int tab_offset;
    int tab_displayed;

    // Syntax highlight
    EditorSyntax* HLDB;

    // File explorer
    EditorExplorer explorer;

    // Console
    int con_front;
    int con_rear;
    int con_size;
    char con_msg[EDITOR_CON_COUNT][EDITOR_CON_LENGTH];

    // Prompt
    char prompt[EDITOR_PROMPT_LENGTH];
    char prompt_right[EDITOR_RIGHT_PROMPT_LENGTH];
} Editor;

// Text editor
extern Editor gEditor;

// Current file
extern EditorFile* gCurFile;

void editorInit(void);
void editorFree(void);
void editorInitFile(EditorFile* file);
void editorFreeFile(EditorFile* file);

// Multiple files control
int editorAddFile(const EditorFile* file);
void editorRemoveFile(int index);
void editorChangeToFile(int index);
void editorNewUntitledFile(EditorFile* file);

#endif
