#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

typedef struct EditorExplorerNodeData {
    struct EditorExplorerNode** nodes;
    size_t count;
} EditorExplorerNodeData;

typedef struct EditorExplorerNode {
    char* filename;
    bool is_directory;
    bool is_open;  // Is directory open in the explorer
    bool loaded;  // Is directory loaded
    size_t dir_count;
    EditorExplorerNodeData dir;
    EditorExplorerNodeData file;
} EditorExplorerNode;

typedef struct EditorFile EditorFile;

bool editorOpen(EditorFile* file, const char* filename);
void editorSave(EditorFile* file, int save_as);
void editorOpenFilePrompt();

EditorExplorerNode* editorExplorerCreate(const char* path);
void editorExplorerLoadNode(EditorExplorerNode* node);
EditorExplorerNode* editorExplorerSearch(int index);
void editorExplorerFree(EditorExplorerNode* node);

#endif
