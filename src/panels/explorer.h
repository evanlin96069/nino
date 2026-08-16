#ifndef PANEL_EXPLORER_H
#define PANEL_EXPLORER_H

#include "ui/panel.h"

typedef struct EditorExplorerNodeData {
    struct EditorExplorerNode** nodes;
    size_t count;
} EditorExplorerNodeData;

typedef struct EditorExplorerNode {
    char* filename;
    bool is_directory;
    bool is_open;  // Is directory open in the explorer
    bool loaded;   // Is directory loaded
    int depth;
    size_t dir_count;
    EditorExplorerNodeData dir;
    EditorExplorerNodeData file;
} EditorExplorerNode;

typedef struct ExplorerPanel {
    Panel base;

    int offset;
    int selected_index;
    EditorExplorerNode* node;  // Root node of explorer tree
    VECTOR(EditorExplorerNode*) flatten;
} ExplorerPanel;

ExplorerPanel* panelExplorerCreate(void);

// Explorer tree
EditorExplorerNode* editorExplorerCreate(const char* path);
void editorExplorerFreeNode(EditorExplorerNode* node);
void editorExplorerRefresh(void);

#endif
