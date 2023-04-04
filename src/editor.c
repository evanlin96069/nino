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

    // Set current file to 0 before load
    editorInitFile(&gEditor.files[0]);
    gCurFile = &gEditor.files[0];

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

    editorInitCommands();
    editorLoadConfig();

    resizeWindow();
    enableAutoResize();

    atexit(terminalExit);

    // Draw loading
    editorRefreshScreen();
}

void editorFree() {
    for (int i = 0; i < gEditor.file_count; i++) {
        editorFreeFile(&gEditor.files[i]);
    }
    editorFreeClipboardContent(&gEditor.clipboard);
}

void editorFreeFile(EditorFile* file) {
    for (int i = 0; i < file->num_rows; i++) {
        editorFreeRow(&file->row[i]);
    }
    editorFreeActionList(file->action_head.next);
    free(file->row);
    free(file->filename);
}

int editorAddFile() {
    if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT)
        return -1;
    EditorFile* file = &gEditor.files[gEditor.file_count++];
    editorInitFile(file);
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
}

void editorInitFile(EditorFile* file) {
    file->cursor.x = 0;
    file->cursor.y = 0;
    file->cursor.is_selected = false;
    file->cursor.select_x = 0;
    file->cursor.select_y = 0;

    file->sx = 0;

    file->bracket_autocomplete = 0;

    file->row_offset = 0;
    file->col_offset = 0;

    file->num_rows = 0;
    file->num_rows_digits = 0;

    file->dirty = 0;
    file->filename = NULL;

    file->row = NULL;

    file->syntax = 0;

    file->action_head.action = NULL;
    file->action_head.next = NULL;
    file->action_head.prev = NULL;
    file->action_current = &file->action_head;
}
