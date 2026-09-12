#ifndef FILE_IO_H
#define FILE_IO_H

typedef struct EditorFile EditorFile;

typedef enum EditorOpenStatus {
    OPEN_FAILED = 0,
    OPEN_FILE,
    OPEN_FILE_NEW,
    OPEN_DIR,
    OPEN_OPENED,
} EditorOpenStatus;
EditorOpenStatus editorLoadFile(EditorFile* file,
                                const char* filename,
                                bool reload);

bool editorSave(EditorFile* file, const char* path);
void editorPromptSaveAs(EditorFile* file);
bool editorIsDangerousSave(const EditorFile* file, bool verbose);

void editorNewUntitledFile(EditorFile* file);
void editorNewUntitledFileFromStdin(EditorFile* file);

void editorPromptFileOpen(void);

typedef enum EditorReloadStatus {
    RELOAD_SUCCESS,
    RELOAD_UNTITLED,
    RELOAD_DIRTY,
    RELOAD_NOT_EXIST,
    RELOAD_DIR,
    RELOAD_FAILED,
} EditorReloadStatus;
EditorReloadStatus editorReloadFile(int file_index, bool force);

#endif
