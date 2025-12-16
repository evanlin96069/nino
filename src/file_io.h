#ifndef FILE_IO_H
#define FILE_IO_H

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
    int prefered_width;
    int width;
    int offset;
    int selected_index;
    EditorExplorerNode* node;  // Root node of explorer tree
    VECTOR(EditorExplorerNode*) flatten;
} EditorExplorer;

typedef struct EditorFile EditorFile;

typedef enum OpenStatus {
    OPEN_FAILED = 0,
    OPEN_FILE,
    OPEN_DIR,
    OPEN_OPENED,
} OpenStatus;

OpenStatus editorOpen(EditorFile* file, const char* filename);
bool editorSave(EditorFile* file, int save_as);
void editorOpenFilePrompt(void);

EditorExplorerNode* editorExplorerCreate(const char* path);
void editorExplorerLoadNode(EditorExplorerNode* node);
void editorExplorerRefresh(void);
void editorExplorerFree(void);

#endif
