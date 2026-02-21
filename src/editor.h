#ifndef EDITOR_H
#define EDITOR_H

#include "action.h"
#include "config.h"
#include "file_io.h"
#include "os.h"
#include "output.h"
#include "row.h"
#include "select.h"

#define EDITOR_FILE_MAX_SLOT 32
#define EDITOR_SPLIT_MAX 2

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

typedef struct EditorTab {
    // File
    int file_index;

    // Cursor position
    EditorCursor cursor;

    // Hidden cursor x position
    int sx;

    // bracket complete level
    int bracket_autocomplete;

    // Editor offsets
    int row_offset;
    int col_offset;
} EditorTab;

typedef struct EditorSplit {
    EditorTab tabs[EDITOR_FILE_MAX_SLOT];
    int tab_count;
    int tab_active_index;
    int tab_offset;
    int tab_displayed;
    float ratio;
} EditorSplit;

typedef struct EditorFile {
    int reference_count;

    // Total line number
    int num_rows;
    int lineno_width;

    // Encoding
    uint8_t newline;

    // File info
    char* filename;  // NULL if untitled
    int new_id;
    bool has_file_info;
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
    // Screen
    ScreenCell** screen;
    ScreenCell** prev_screen;
    int screen_rows;
    int screen_cols;
    int old_screen_rows;
    int old_screen_cols;
    bool screen_size_updated;

    // Text field size
    int display_rows;

    // Editor mode
    int state;
    bool mouse_mode;

    // Cursor position for prompt
    int px;

    // Copy paste
    EditorClipboard clipboard;
    bool copy_line;

    // Color settings
    EditorColorScheme color_cfg;

    // ConCmd linked list
    EditorConCmd* cvars;

    // Files
    EditorFile files[EDITOR_FILE_MAX_SLOT];
    int file_count;

    // Splits
    EditorSplit splits[EDITOR_SPLIT_MAX];
    int split_count;
    int split_active_index;

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

static inline EditorFile* editorTabGetFile(const EditorTab* tab) {
    return &gEditor.files[tab->file_index];
}

static inline EditorTab* editorSplitGetTab(int split_index) {
    EditorSplit* split = &gEditor.splits[split_index];
    return &split->tabs[split->tab_active_index];
}

static inline EditorSplit* editorGetActiveSplit(void) {
    return &gEditor.splits[gEditor.split_active_index];
}

static inline EditorTab* editorGetActiveTab(void) {
    EditorSplit* split = editorGetActiveSplit();
    return &split->tabs[split->tab_active_index];
}

static inline EditorFile* editorGetActiveFile(void) {
    return editorTabGetFile(editorGetActiveTab());
}

static inline void editorUpdateSx(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);
    tab->sx = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
}

void editorInit(void);
void editorFree(void);
void editorInitFile(EditorFile* file);
void editorFreeFile(EditorFile* file);

// Multiple files control
int editorAddFileToActiveSplit(EditorFile* file);
int editorAddFile(EditorFile* file);
void editorRemoveFile(int file_index);

int editorAddTab(int split_index, int file_index);
void editorRemoveTab(int split_index, int tab_index);
int editorFindTabByFileIndex(int split_index, int file_index);
void editorChangeToFile(int split_index, int tab_index);

int editorAddSplit(void);
void editorRemoveSplit(int split_index);

#endif
