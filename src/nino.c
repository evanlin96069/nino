#include <string.h>

#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "output.h"
#include "row.h"

int main(int argc, char* argv[]) {
    editorInit();
    EditorFile file = {0};

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT)
                break;
            memset(&file, 0, sizeof(EditorFile));
            if (editorOpen(&file, argv[i])) {
                editorAddFile(&file);
            }
        }
    }

    if (gEditor.file_count == 0) {
        if (gEditor.explorer.node) {
            gEditor.explorer.focused = true;
        } else {
            editorAddFile(&file);
            editorInsertRow(gCurFile, 0, "", 0);
        }
    }

    if (gEditor.explorer.node == NULL)
        gEditor.explorer.width = 0;

    gEditor.loading = false;

    while (gEditor.file_count || gEditor.explorer.node) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    editorFree();
    return 0;
}
