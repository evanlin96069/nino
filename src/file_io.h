#ifndef FILE_IO_H
#define FILE_IO_H

typedef struct EditorFile EditorFile;

typedef enum OpenStatus {
    OPEN_FAILED = 0,
    OPEN_FILE,
    OPEN_FILE_NEW,
    OPEN_DIR,
    OPEN_OPENED,
} OpenStatus;

OpenStatus editorLoadFile(EditorFile* file, const char* filename, bool reload);
bool editorSave(EditorFile* file, const char* path);
void editorPromptSaveAs(EditorFile* file);
bool editorIsDangerousSave(const EditorFile* file, bool verbose);
void editorNewUntitledFile(EditorFile* file);
void editorNewUntitledFileFromStdin(EditorFile* file);
void editorPromptFileOpen(void);

#endif
