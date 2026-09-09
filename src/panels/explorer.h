#ifndef PANEL_EXPLORER_H
#define PANEL_EXPLORER_H

#include "os.h"
#include "utils.h"

#include "ui/panel.h"

typedef struct EditorExplorerNode EditorExplorerNode;
typedef VECTOR(EditorExplorerNode*) VecEditorExplorerNode;

struct EditorExplorerNode {
    char* filename;
    int depth;

    bool is_directory;

    // Directory only
    bool is_open;
    bool loaded;
    FileInfo info;
    VecEditorExplorerNode dir_nodes;
    VecEditorExplorerNode file_nodes;
};

typedef struct ExplorerPanel {
    Panel base;

    int offset;
    int selected_index;
    EditorExplorerNode* node;  // Root node of explorer tree
    VECTOR(EditorExplorerNode*) flatten;
} ExplorerPanel;

ExplorerPanel* panelExplorerCreate(void);

// Explorer tree
EditorExplorerNode* editorExplorerCreate(const char* path, bool is_directory);
void editorExplorerFreeNode(EditorExplorerNode* node);
void editorExplorerRefresh(void);
void editorExplorerReload(void);

void editorExplorerSetSide(bool left);

#endif
