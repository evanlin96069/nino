#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "highlight.h"
#include "prompt.h"

Editor gEditor;
EditorFile* gCurFile;

void editorInit(void) {
    memset(&gEditor, 0, sizeof(Editor));
    gEditor.loading = true;
    gEditor.state = EDIT_MODE;
    gEditor.mouse_mode = true;

    gEditor.color_cfg = color_default;

    gEditor.con_front = -1;

    editorRegisterCommands();
    editorInitHLDB();

    gEditor.explorer.prefered_width = gEditor.explorer.width =
        CONVAR_GETINT(ex_default_width);

    memset(&gEditor.files[0], 0, sizeof(EditorFile));
    gCurFile = &gEditor.files[0];
}

void editorFree(void) {
    for (int i = 0; i < gEditor.file_count; i++) {
        editorFreeFile(&gEditor.files[i]);
    }
    editorFreeClipboardContent(&gEditor.clipboard);
    editorExplorerFree();
    editorFreeHLDB();
    editorUnregisterCommands();
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

int editorAddFile(const EditorFile* file) {
    if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT) {
        editorMsg("Already opened too many files!");
        return -1;
    }

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

void editorNewUntitledFile(EditorFile* file) {
    editorInitFile(file);
    editorInsertRow(file, 0, "", 0);
    file->new_id = gEditor.new_file_count;
    gEditor.new_file_count++;
}