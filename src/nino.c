#include "buildnum.h"
#include "config.h"
#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "opt.h"
#include "output.h"
#include "row.h"
#include "terminal.h"

int main(int argc, char* argv[]) {
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

    EditorFile file;
    for (int i = 0; i < argc; i++) {
        if (editorOpen(&file, argv[i]) == OPEN_FILE) {
            int file_index = editorAddFile(&file);
            if (file_index == -1) {
                break;
            }
            if (editorAddTab(file_index) == -1) {
                editorRemoveFile(file_index);
                break;
            }
        }
    }

    gEditor.state = EDIT_MODE;
    if (gEditor.tab_count == 0) {
        if (gEditor.explorer.node) {
            gEditor.state = EXPLORER_MODE;
        } else {
            editorNewUntitledFile(&file);
            int file_index = editorAddFile(&file);
            if (file_index != -1) {
                if (editorAddTab(file_index) == -1) {
                    editorRemoveFile(file_index);
                }
            }
        }
    }

    gEditor.explorer.prefered_width = CONVAR_GETINT(ex_default_width);
    if (gEditor.explorer.node == NULL) {
        gEditor.explorer.width = 0;
    } else {
        gEditor.explorer.width = gEditor.explorer.prefered_width;
    }

    while (gEditor.file_count || gEditor.explorer.node) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

DONE:
    argsFree(argc_utf8, argv_utf8);
    editorFree();
    return 0;
}
