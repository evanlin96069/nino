#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "output.h"
#include "row.h"

int main(int argc, char* argv[]) {
    editorInit();
    EditorFile file;

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT)
                    break;
            editorInitFile(&file);
            if (editorOpen(&file, argv[i])) {
                editorAddFile(&file);
            }
        }
    }

    if (gEditor.file_count == 0) {
        editorInitFile(&file);
        editorInsertRow(gCurFile, 0, "", 0);
        editorAddFile(&file);
    }

    gEditor.loading = false;

    while (gEditor.file_count) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    editorFree();
    return 0;
}
