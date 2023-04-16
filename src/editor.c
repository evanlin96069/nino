#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"
#include "output.h"
#include "row.h"
#include "terminal.h"
#include "utils.h"

Editor gEditor;
EditorFile* gCurFile;

void editorInit() {
    enableRawMode();
    enableSwap();

    gEditor.file_count = 0;
    gEditor.file_index = 0;
    gEditor.tab_offset = 0;
    gEditor.tab_displayed = 0;

    gEditor.screen_rows = 0;
    gEditor.screen_cols = 0;

    gEditor.loading = true;
    gEditor.state = EDIT_MODE;
    gEditor.mouse_mode = false;
    // Mouse mode default on
    enableMouse();

    gEditor.px = 0;

    gEditor.clipboard.size = 0;
    gEditor.clipboard.data = NULL;

    gEditor.cvars = NULL;

    gEditor.color_cfg = color_default;

    gEditor.status_msg[0][0] = '\0';
    gEditor.status_msg[1][0] = '\0';

    gEditor.explorer_focus = false;
    gEditor.explorer_node = NULL;

    editorInitCommands();
    editorLoadConfig();

    resizeWindow();
    gEditor.explorer_prefer_width = gEditor.explorer_width =
        gEditor.screen_cols * 0.2f;

    enableAutoResize();

    atexit(terminalExit);

    // Draw loading
    memset(&gEditor.files[0], 0, sizeof(EditorFile));
    gCurFile = &gEditor.files[0];
    editorRefreshScreen();
}

void editorFree() {
    for (int i = 0; i < gEditor.file_count; i++) {
        editorFreeFile(&gEditor.files[i]);
    }
    editorFreeClipboardContent(&gEditor.clipboard);
    editorExplorerFree(gEditor.explorer_node);
}

void editorFreeFile(EditorFile* file) {
    for (int i = 0; i < file->num_rows; i++) {
        editorFreeRow(&file->row[i]);
    }
    editorFreeActionList(file->action_head.next);
    free(file->row);
    free(file->filename);
}

int editorAddFile(EditorFile* file) {
    if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT)
        return -1;
    EditorFile* current = &gEditor.files[gEditor.file_count];

    *current = *file;
    current->action_current = &current->action_head;

    gEditor.file_count++;
    return gEditor.file_count - 1;
}

void editorRemoveFile(int index) {
    if (index < 0 || index > gEditor.file_count)
        return;

    EditorFile* file = &gEditor.files[index];
    editorFreeFile(file);
    if (file == &gEditor.files[gEditor.file_count]) {
        // file is at the end
        gEditor.file_count--;
        return;
    }
    memmove(file, &gEditor.files[index + 1],
            sizeof(EditorFile) * (gEditor.file_count - index));
    gEditor.file_count--;
}

void editorChangeToFile(int index) {
    if (index < 0 || index >= gEditor.file_count)
        return;
    gEditor.file_index = index;
    gCurFile = &gEditor.files[index];

    if (gEditor.tab_offset > index ||
        gEditor.tab_offset + gEditor.tab_displayed <= index) {
        gEditor.tab_offset = index;
    }
}
