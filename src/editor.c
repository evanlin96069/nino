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
    gEditor.pending_input.type = UNKNOWN;

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
    editorFreeScreen(gEditor.screen_rows);
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

int editorAddFileToActiveSplit(EditorFile* file) {
    int file_index = editorAddFile(file);
    if (file_index != -1) {
        if (gEditor.split_count == 0) {
            editorAddSplit();
        }

        int tab_index = editorAddTab(gEditor.split_active_index, file_index);
        if (tab_index != -1) {
            return file_index;
        }
        editorRemoveFile(file_index);
    }
    return -1;
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

int editorAddTab(int split_index, int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return -1;
    if (split_index < 0 || split_index >= gEditor.split_count)
        return -1;

    EditorSplit* split = &gEditor.splits[split_index];

    if (split->tab_count >= EDITOR_FILE_MAX_SLOT) {
        editorMsg("Already opened too many tabs!");
        return -1;
    }

    EditorFile* file = &gEditor.files[file_index];
    EditorTab* tab = &split->tabs[split->tab_count];
    memset(tab, 0, sizeof(EditorTab));
    tab->file_index = file_index;

    if (file->reference_count == 0) {
        gEditor.file_count++;
    }
    file->reference_count++;

    split->tab_active_index = split->tab_count;
    split->tab_count++;

    int index = split->tab_count - 1;
    if (gEditor.state != LOADING_MODE) {
        // hack: refresh screen to update tab_displayed
        editorRefreshScreen();
        editorChangeToFile(split_index, index);
    }

    return index;
}

// Won't update tab_active_index
void editorRemoveTab(int split_index, int tab_index) {
    if (split_index < 0 || split_index >= gEditor.split_count)
        return;

    EditorSplit* split = &gEditor.splits[split_index];

    if (tab_index < 0 || tab_index >= split->tab_count)
        return;

    int file_index = split->tabs[tab_index].file_index;
    editorRemoveFile(file_index);

    if (tab_index != split->tab_count - 1) {
        // Move the later tabs forward
        memmove(&split->tabs[tab_index], &split->tabs[tab_index + 1],
                sizeof(EditorTab) * (split->tab_count - tab_index - 1));
    }

    split->tab_count--;

    // Close split if no file in the tab
    if (split->tab_count == 0) {
        editorRemoveSplit(split_index);
    }
}

int editorFindTabByFileIndex(int split_index, int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return -1;
    if (split_index < 0 || split_index >= gEditor.split_count)
        return -1;

    EditorSplit* split = &gEditor.splits[split_index];

    for (int i = 0; i < split->tab_count; i++) {
        if (split->tabs[i].file_index == file_index) {
            return i;
        }
    }
    return -1;
}

void editorChangeToFile(int split_index, int tab_index) {
    if (split_index < 0 || split_index >= gEditor.split_count)
        return;

    EditorSplit* split = &gEditor.splits[split_index];

    if (tab_index < 0 || tab_index >= split->tab_count)
        return;

    split->tab_active_index = tab_index;

    if (split->tab_offset > tab_index ||
        split->tab_offset + split->tab_displayed <= tab_index) {
        split->tab_offset = tab_index;
    }
}

int editorAddSplit(void) {
    if (gEditor.split_count >= EDITOR_SPLIT_MAX)
        return -1;

    int index;
    if (gEditor.split_count == 0) {
        index = 0;
        gEditor.split_active_index = index;
    } else {
        index = gEditor.split_active_index + 1;
        memmove(&gEditor.splits[index + 1], &gEditor.splits[index],
                sizeof(EditorSplit) * (gEditor.split_count - index));
    }

    memset(&gEditor.splits[index], 0, sizeof(EditorSplit));
    gEditor.splits[index].ratio = 1.0f;
    gEditor.split_count++;

    return index;
}

void editorRemoveSplit(int split_index) {
    if (split_index < 0 || split_index >= gEditor.split_count)
        return;

    EditorSplit* split = &gEditor.splits[split_index];
    for (int i = 0; i < split->tab_count; i++) {
        editorRemoveFile(split->tabs[i].file_index);
    }

    memmove(&gEditor.splits[split_index], &gEditor.splits[split_index + 1],
            sizeof(EditorSplit) * (gEditor.split_count - split_index - 1));
    gEditor.split_count--;

    // Adjust active index
    int active_index = gEditor.split_active_index;
    if (active_index == split_index) {
        if (active_index > 0) {
            active_index--;
        }
    } else if (active_index > split_index) {
        active_index--;
    }
    gEditor.split_active_index = active_index;
}
