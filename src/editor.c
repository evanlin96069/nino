#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "highlight.h"
#include "os.h"
#include "output.h"
#include "prompt.h"

Editor gEditor;

void editorInit(void) {
    memset(&gEditor, 0, sizeof(Editor));
    gEditor.state = LOADING_MODE;
    gEditor.mouse_mode = true;

    gEditor.color_cfg = color_default;

    gEditor.con_front = -1;

    osInit();

    editorRegisterCommands();
    editorInitHLDB();

    editorLoadInitConfig();

    memset(&gEditor.files[0], 0, sizeof(EditorFile));
}

void editorFree(void) {
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count > 0) {
            editorFreeFile(&gEditor.files[i]);
            gEditor.files[i].reference_count = 0;
        }
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

void editorRemoveFile(int index) {
    if (index < 0 || index >= EDITOR_FILE_MAX_SLOT)
        return;

    EditorFile* file = &gEditor.files[index];
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

int editorAddTab(int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return -1;

    if (gEditor.tab_count >= EDITOR_FILE_MAX_SLOT) {
        editorMsg("Already opened too many tabs!");
        return -1;
    }

    EditorFile* file = &gEditor.files[file_index];
    EditorTab* tab = &gEditor.tabs[gEditor.tab_count];
    memset(tab, 0, sizeof(EditorTab));
    tab->file_index = file_index;

    if (file->reference_count == 0) {
        gEditor.file_count++;
    }
    file->reference_count++;

    gEditor.tab_count++;

    int index = gEditor.tab_count - 1;
    if (gEditor.state != LOADING_MODE) {
        // hack: refresh screen to update gEditor.tab_displayed
        editorRefreshScreen();
        editorChangeToFile(index);
    }

    return index;
}

void editorRemoveTab(int index) {
    if (index < 0 || index >= gEditor.tab_count)
        return;

    int file_index = gEditor.tabs[index].file_index;
    editorRemoveFile(file_index);

    if (index == gEditor.tab_count - 1) {
        gEditor.tab_count--;
        return;
    }

    memmove(&gEditor.tabs[index], &gEditor.tabs[index + 1],
            sizeof(EditorTab) * (gEditor.tab_count - index - 1));
    gEditor.tab_count--;
}

int editorFindTabByFileIndex(int file_index) {
    for (int i = 0; i < gEditor.tab_count; i++) {
        if (gEditor.tabs[i].file_index == file_index) {
            return i;
        }
    }
    return -1;
}

void editorChangeToFile(int index) {
    if (index < 0 || index >= gEditor.tab_count)
        return;
    gEditor.tab_active_index = index;

    if (gEditor.tab_offset > index ||
        gEditor.tab_offset + gEditor.tab_displayed <= index) {
        gEditor.tab_offset = index;
    }
}

static int findAvailableUntitledId(void) {
    for (int id = 0;; id++) {
        int used = 0;
        for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
            const EditorFile* open_file = &gEditor.files[i];
            if (open_file->reference_count == 0)
                continue;
            if (!open_file->filename && open_file->new_id == id) {
                used = 1;
                break;
            }
        }
        if (!used) {
            return id;
        }
    }
}

void editorNewUntitledFile(EditorFile* file) {
    editorInitFile(file);
    editorInsertRow(file, 0, "", 0);
    file->new_id = findAvailableUntitledId();
}
