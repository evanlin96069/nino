#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#include "utils.h"

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

typedef struct EditorExplorer {
    bool focused;
    int prefered_width;
    int width;
    int offset;
    uint32_t selected_index;
    EditorExplorerNode* node;  // Root node of explorer tree
    VECTOR(EditorExplorerNode*) flatten;
} EditorExplorer;

typedef struct EditorFile EditorFile;

bool editorOpen(EditorFile* file, const char* filename);
void editorSave(EditorFile* file, int save_as);
void editorOpenFilePrompt(void);

EditorExplorerNode* editorExplorerCreate(const char* path);
void editorExplorerLoadNode(EditorExplorerNode* node);
void editorExplorerRefresh(void);
void editorExplorerFree(void);

#endif
