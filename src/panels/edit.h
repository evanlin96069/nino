#ifndef PANEL_EDIT_H
#define PANEL_EDIT_H

#include "ui/panel.h"

#include "action.h"
#include "editor.h"
#include "row.h"
#include "utils.h"

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

    // Find
    bool has_match;
    int match_row;
    uint32_t match_col;
    uint32_t match_len;
} EditorTab;

typedef enum EditWaitState {
    EDIT_WAIT_NONE,
    // Close tab with Ctrl+W, wait for another Ctrl+W to confirm
    EDIT_WAIT_CLOSE,
    // Save read-only or modified file with Ctrl+S, wait for another Ctrl+S to
    // confirm
    EDIT_WAIT_SAVE,
    // mouse3 press, wait for mouse3 release (must be on the same tab)
    EDIT_WAIT_CLOSE_RELEASE1,
    // mouse3 release, file dirty, wait for another mouse3 to confirm
    EDIT_WAIT_CLOSE_PRESS2,
    // Second mouse3 press, wait for mouse3 release to confirm
    EDIT_WAIT_CLOSE_RELEASE2,
} EditWaitState;

typedef enum EditMouseMode {
    EDIT_MOUSE_NONE,
    EDIT_MOUSE_TAB_BAR,
    EDIT_MOUSE_SELECT_DRAG,
    EDIT_MOUSE_TAB_DRAG,  // Not implemented yet
} EditMouseMode;

typedef struct EditPanel {
    Panel base;

    VECTOR(EditorTab) tabs;
    int tab_active_index;
    int tab_offset;
    int tab_displayed;

    EditMouseMode mouse_mode;
    EditWaitState wait_state;
    int wait_tab_index;
} EditPanel;

EditPanel* panelEditCreate(void);

static inline EditorFile* editorTabGetFile(EditorTab* tab) {
    if (!tab)
        return NULL;
    if (tab->file_index < 0 || tab->file_index >= EDITOR_FILE_MAX_SLOT)
        return NULL;
    EditorFile* file = &gEditor.files[tab->file_index];
    if (file->reference_count == 0)
        return NULL;
    return file;
}

static inline const EditorFile* editorTabGetFileConst(const EditorTab* tab) {
    if (!tab)
        return NULL;
    if (tab->file_index < 0 || tab->file_index >= EDITOR_FILE_MAX_SLOT)
        return NULL;
    EditorFile* file = &gEditor.files[tab->file_index];
    if (file->reference_count == 0)
        return NULL;
    return file;
}

static inline EditorTab* editorSplitGetTab(EditPanel* split) {
    if (!split)
        return NULL;
    if (split->tab_active_index < 0 ||
        (uint32_t)split->tab_active_index >= split->tabs.size)
        return NULL;
    return &split->tabs.data[split->tab_active_index];
}

static inline const EditorTab* editorSplitGetTabConst(const EditPanel* split) {
    if (!split)
        return NULL;
    if (split->tab_active_index < 0 ||
        (uint32_t)split->tab_active_index >= split->tabs.size)
        return NULL;
    return &split->tabs.data[split->tab_active_index];
}

static inline EditorTab* editorGetActiveTab(void) {
    if (!gEditor.active_edit_panel)
        return NULL;
    return editorSplitGetTab(gEditor.active_edit_panel);
}

static inline EditorFile* editorGetActiveFile(void) {
    return editorTabGetFile(editorGetActiveTab());
}

static inline void editorUpdateSx(EditorTab* tab) {
    const EditorFile* file = editorTabGetFile(tab);
    tab->sx = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x);
}

static inline void editorFocusActiveSplit(void) {
    if (gEditor.active_edit_panel) {
        uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.active_edit_panel);
    } else {
        uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.welcome_panel);
    }
}

int editorAddFileToActiveSplit(EditorFile* file);
int editorAddTab(EditPanel* split, int file_index);
void editorCloseTab(EditPanel* split, int tab_index);
int editorFindTabByFileIndex(EditPanel* split, int file_index);
void editorChangeToFile(EditPanel* split, int tab_index);

EditPanel* editorAddSplit(EditPanel* relative_to, bool leftright);
void editorRemoveSplit(EditPanel* split);

void editorScroll(EditPanel* split, int dist);
void editorScrollToCursor(EditPanel* split);
void editorScrollToCursorCenter(EditPanel* split);

void editorCancelPendingWait(EditPanel* split);

#endif
