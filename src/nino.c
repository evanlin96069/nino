#include "buildnum.h"
#include "config.h"
#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "opt.h"
#include "os.h"
#include "output.h"
#include "prompt.h"
#include "row.h"
#include "terminal.h"

int main(int argc, char* argv[]) {
    osInit();
    editorInit();

    int argc_utf8 = argc;
    char** argv_utf8 = argv;
    argsInit(&argc_utf8, &argv_utf8);
    argc = argc_utf8;
    argv = argv_utf8;
    FOR_OPTS(argc, argv) {
        case 'v':
            printf("Exe version %s (%s)\n", EDITOR_VERSION, EDITOR_NAME);
            printf("Exe build: %s %s (%d)\n", editor_build_time,
                   editor_build_date, editorGetBuildNumber());
            goto DONE;
        case 'c':
            editorCmd(OPTARG(argc, argv));
            break;
    }

    editorInitTerminal();
    editorLoadInitConfig();
    editorRefreshScreen();  // Draw loading

    EditorFile file;
    if (argc > 0) {
        for (int i = 0; i < argc; i++) {
            if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT) {
                editorMsg("Already opened too many files!");
                break;
            }
            if (editorOpen(&file, argv[i])) {
                editorAddFile(&file);
            }
        }
    }

    if (gEditor.file_count == 0) {
        if (gEditor.explorer.node) {
            gEditor.state = EXPLORER_MODE;
        } else {
            editorInitFile(&file);
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

DONE:
    argsFree(argc_utf8, argv_utf8);
    editorFree();
    return 0;
}
