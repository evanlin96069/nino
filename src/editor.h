#ifndef EDITOR_H
#define EDITOR_H

#include "action.h"
#include "config.h"
#include "file_io.h"
#include "os.h"
#include "row.h"
#include "select.h"

#include "ui/compositor.h"

#define EDITOR_FILE_MAX_SLOT 32

#define EDITOR_CON_COUNT 16
#define EDITOR_CON_LENGTH 255

typedef enum EditorState {
    STATE_EXIT = -1,
    STATE_LOADING,
    STATE_RUNNING,
} EditorState;

typedef enum EditorHelpMsg {
    HELP_NONE,
    HELP_GLOBAL,
    HELP_EDIT,
    HELP_FIND_PROMPT,
    HELP_GOTO_PROMPT,
    HELP_OPEN_PROMPT,
    HELP_CONFIG_PROMPT,
    HELP_SAVE_AS_PROMPT,

    HELP_COUNT,
} EditorHelpMsg;

typedef struct EditorSyntax EditorSyntax;

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
    bool read_only;
    bool unlocked;  // Read-only but unlocked by user

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

typedef struct ExplorerPanel ExplorerPanel;
typedef struct WelcomePanel WelcomePanel;
typedef struct PromptPanel PromptPanel;
typedef struct EditPanel EditPanel;

typedef struct Editor {
    bool pending_quit_confirm;

    // Screen
    bool screen_size_updated;
    int screen_rows;
    int screen_cols;

    Surface screen;
    Surface old_screen;
    abuf render_buffer;

    UI ui;
    ExplorerPanel* explorer_panel;
    WelcomePanel* welcome_panel;
    PromptPanel* prompt_panel;
    EditPanel* active_edit_panel;
    EditPanel* pending_edit_panel;  // EditWaitState
    int split_count;

    // Editor mode
    EditorState state;
    bool mouse_mode;

    // Copy paste
    EditorClipboard clipboard;
    bool copy_line;

    // Color settings
    Color color_cfg[UI_COLOR_COUNT];

    // ConCmd linked list
    ConCommandBase* cvars;

    // Files
    EditorFile files[EDITOR_FILE_MAX_SLOT];
    int file_count;

    // Syntax highlight
    EditorSyntax* HLDB;

    // Console
    int con_front;
    int con_rear;
    int con_size;
    bool con_keep_msg;
    char con_msg[EDITOR_CON_COUNT][EDITOR_CON_LENGTH];

    // Help
    EditorHelpMsg help_msg;
    EditorHelpMsg help_msg_prev;
} Editor;

// Text editor
extern Editor gEditor;

void editorInit(void);
void editorFree(void);

// File
void editorInitFile(EditorFile* file);
void editorFreeFile(EditorFile* file);
int editorAddFile(EditorFile* file);
void editorRemoveFile(int file_index);
int editorGetDirtyFileCount(void);

// Help
const char* editorHelpMsgToString(EditorHelpMsg msg);
void editorHelpSetMsg(EditorHelpMsg msg);
void editorHelpRestoreMsg(void);

#endif
