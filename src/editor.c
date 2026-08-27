#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "console.h"
#include "highlight.h"
#include "os.h"
#include "output.h"

#include "panels/explorer.h"
#include "panels/prompt.h"
#include "panels/welcome.h"

Editor gEditor;

static void editorLayoutInit(void) {
    gEditor.ui.root = layoutNodeCreate(LAYOUT_TOPBOTTOM);
    gEditor.ui.root->resizable = false;

    // Root top
    LayoutNode* top_node = layoutNodeCreate(LAYOUT_LEFTRIGHT);

    ExplorerPanel* explorer_panel = panelExplorerCreate();
    LayoutNode* explorer_node = layoutNodeCreateLeaf((Panel*)explorer_panel);
    explorer_node->size_type = LAYOUT_SIZE_FIXED;
    explorer_node->fixed_size = ex_default_width.int_value;
    explorer_node->enabled = false;

    WelcomePanel* welcome_panel = panelWelcomeCreate();
    LayoutNode* welcome_node = layoutNodeCreateLeaf((Panel*)welcome_panel);

    layoutAppendChild(top_node, explorer_node);
    layoutAppendChild(top_node, welcome_node);
    // Edit panels will be created on file open

    // Root bottom
    PromptPanel* prompt_panel = panelPromptCreate();
    LayoutNode* prompt_node = layoutNodeCreateLeaf((Panel*)prompt_panel);
    prompt_node->size_type = LAYOUT_SIZE_FIXED;
    prompt_node->fixed_size = 1;
    prompt_node->resizable = false;
    prompt_node->enabled = false;

    layoutAppendChild(gEditor.ui.root, top_node);
    layoutAppendChild(gEditor.ui.root, prompt_node);

    layoutUpdate(gEditor.ui.root);

    gEditor.explorer_panel = explorer_panel;
    gEditor.welcome_panel = welcome_panel;
    gEditor.prompt_panel = prompt_panel;
    gEditor.active_edit_panel = NULL;
}

void editorInit(void) {
    gEditor.state = STATE_LOADING;
    gEditor.mouse_mode = true;
    memcpy(gEditor.color_cfg, color_default, sizeof(gEditor.color_cfg));
    gEditor.con_front = -1;

    osInit();

    editorOutputInit();

    editorRegisterCommands();
    editorInitHLDB();

    uiInit(&gEditor.ui);
    editorLayoutInit();
}

void editorFree(void) {
    uiFree(&gEditor.ui);

#ifndef NDEBUG
    // Check if any files are somehow not associated with any splits
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count > 0) {
            PANIC(
                "File(s) still open after uiFree. This indicates a memory "
                "leak.");
        }
    }
#endif

    vector_free(gEditor.recent_splits);
    editorFreeClipboardContent(&gEditor.clipboard);
    editorFreeHLDB();
    editorUnregisterCommands();

    editorOutputFree();

    osDeinit();
}

void editorInitFile(EditorFile* file) {
    memset(file, 0, sizeof(EditorFile));
    file->newline = editorGetDefaultNewline();
}

void editorFreeFile(EditorFile* file) {
    for (int i = 0; i < file->num_rows; i++) {
        editorFreeRow(&file->row[i]);
    }
    editorFreeActionList(file->action_head);
    free(file->row);
    free(file->filename);
}

int editorAddFile(EditorFile* file) {
    int index = -1;
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        editorMsg("Already opened too many files!");
        editorFreeFile(file);
        return -1;
    }

    EditorFile* current = &gEditor.files[index];

    *current = *file;
    current->action_head = calloc_s(1, sizeof(EditorActionList));
    current->action_current = current->action_head;
    current->reference_count = 0;

    return index;
}

void editorRemoveFile(int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return;

    EditorFile* file = &gEditor.files[file_index];
    if (file->reference_count <= 0) {
        // Likely during the file creation
        if (file->row || file->filename || file->action_head) {
            editorFreeFile(file);
            memset(file, 0, sizeof(EditorFile));
        }
        return;
    }

    file->reference_count--;
    if (file->reference_count == 0) {
        editorFreeFile(file);
        memset(file, 0, sizeof(EditorFile));
        gEditor.file_count--;
    }
}

int editorGetDirtyFileCount(void) {
    int count = 0;
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count > 0 && gEditor.files[i].dirty) {
            count++;
        }
    }
    return count;
}

const char* editorHelpMsgToString(EditorHelpMsg msg) {
    switch (msg) {
        case HELP_GLOBAL:
            if (panelIsEnabled((Panel*)gEditor.welcome_panel) &&
                intro.int_value) {
                // Intro already shows the help message
                return "";
            }
            return " ^Q: Quit  ^O: Open  ^P: Prompt";

        case HELP_EDIT:
            return " ^Q: Quit  ^O: Open  ^P: Prompt  ^S: Save  ^F: Find  ^G: "
                   "Goto";

        case HELP_FIND_PROMPT:
            return " ^Q: Cancel  Up: Back  Down: Next";

        case HELP_GOTO_PROMPT:
        case HELP_OPEN_PROMPT:
        case HELP_CONFIG_PROMPT:
        case HELP_SAVE_AS_PROMPT:
            return " ^Q: Cancel";

        default:
            return "";
    }
}

void editorHelpSetMsg(EditorHelpMsg msg) {
    gEditor.help_msg_prev = gEditor.help_msg;
    gEditor.help_msg = msg;
}

void editorHelpRestoreMsg(void) {
    gEditor.help_msg = gEditor.help_msg_prev;
}

void editorResizeWindow(void) {
    int rows = 0;
    int cols = 0;

    if (getWindowSize(&rows, &cols) == -1)
        PANIC("Unable to query terminal window size");
    editorSetWindowSize(rows, cols);
    gEditor.screen_size_updated = true;
    editorRefreshScreen();
}

void editorSetWindowSize(int rows, int cols) {
    rows = rows < 1 ? 1 : rows;
    cols = cols < 1 ? 1 : cols;

    if (gEditor.screen_rows != rows || gEditor.screen_cols != cols) {
        gEditor.screen_rows = rows;
        gEditor.screen_cols = cols;
        gEditor.screen_size_updated = true;
    }
}
