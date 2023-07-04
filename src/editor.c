#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"
#include "highlight.h"
#include "output.h"
#include "row.h"
#include "terminal.h"
#include "utils.h"

#ifdef _WIN32
HANDLE hStdin = INVALID_HANDLE_VALUE;
HANDLE hStdout = INVALID_HANDLE_VALUE;
#endif

Editor gEditor;
EditorFile* gCurFile;

void editorInit(void) {
#ifdef _WIN32
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdin == INVALID_HANDLE_VALUE)
        PANIC("GetStdHandle(STD_INPUT_HANDLE)");
    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE)
        PANIC("GetStdHandle(STD_OUTPUT_HANDLE)");
#endif

    enableRawMode();
    enableSwap();

    memset(&gEditor, 0, sizeof(Editor));

    gEditor.loading = true;
    gEditor.state = EDIT_MODE;

    // Mouse mode default on
    enableMouse();

    gEditor.color_cfg = color_default;

    editorInitCommands();
    editorLoadDefaultConfig();
    editorLoadDefaultHLDB();

    resizeWindow();
    gEditor.explorer.prefered_width = gEditor.explorer.width =
        gEditor.screen_cols * 0.2f;

#ifndef _WIN32
    enableAutoResize();
#endif

    atexit(terminalExit);

    // Draw loading
    memset(&gEditor.files[0], 0, sizeof(EditorFile));
    gCurFile = &gEditor.files[0];
    editorRefreshScreen();
}

void editorFree(void) {
    for (int i = 0; i < gEditor.file_count; i++) {
        editorFreeFile(&gEditor.files[i]);
    }
    editorFreeClipboardContent(&gEditor.clipboard);
    editorExplorerFree();
    editorFreeHLDB();
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
    if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT)
        return -1;
    EditorFile* current = &gEditor.files[gEditor.file_count];

    *current = *file;
    current->action_head = calloc_s(1, sizeof(EditorActionList));
    current->action_current = current->action_head;

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
