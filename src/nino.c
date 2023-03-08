#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "output.h"
#include "row.h"

int main(int argc, char* argv[]) {
    editorInit();

    if (argc < 2) {
        editorChangeToFile(editorAddFile());
        editorInsertRow(0, "", 0);
    } else {
        for (int i = 1; i < argc; i++) {
            int index = editorAddFile();
            if (index == -1)
                break;
            editorChangeToFile(index);
            if (!editorOpen(gCurFile, argv[i])) {
                editorInsertRow(0, "", 0);
            }
        }
    }

    editorChangeToFile(0);
    gEditor.loading = false;

    while (gEditor.file_count) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    editorFree();
    return 0;
}
