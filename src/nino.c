#include <string.h>

#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "os.h"
#include "output.h"
#include "prompt.h"
#include "row.h"

int main(int argc, char* argv[]) {
    editorInit();
    EditorFile file;
    editorInitFile(&file);

    Args args = argsGet(argc, argv);

    if (args.count > 1) {
        for (int i = 1; i < args.count; i++) {
            if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT) {
                editorMsg("Already opened too many files!");
                break;
            }
            editorInitFile(&file);
            if (editorOpen(&file, args.args[i])) {
                editorAddFile(&file);
            }
        }
    }

    argsFree(args);

    if (gEditor.file_count == 0) {
        if (gEditor.explorer.node) {
            gEditor.state = EXPLORER_MODE;
        } else {
            editorAddFile(&file);
            editorInsertRow(gCurFile, 0, "", 0);
        }
    }

    if (gEditor.explorer.node == NULL) {
        gEditor.explorer.width = 0;
    }

    gEditor.loading = false;

    while (gEditor.file_count || gEditor.explorer.node) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    editorFree();
    return 0;
}
