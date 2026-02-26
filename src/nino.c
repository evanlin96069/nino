#include "buildnum.h"
#include "config.h"
#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "opt.h"
#include "os.h"
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

    EditorFile file;
    bool stdin_piped = false;
    bool is_tty = isStdinTty();
    if ((argc == 0 && !is_tty) || (argc == 1 && strcmp(argv[0], "-") == 0)) {
        stdin_piped = true;
        if (is_tty) {
            fprintf(stderr, "Reading data from keyboard...\n");
        }

        editorNewUntitledFileFromStdin(&file);
        editorAddFileToActiveSplit(&file);
    }

    editorInitTerminal();

    if (!stdin_piped) {
        for (int i = 0; i < argc; i++) {
            if (editorLoadFile(&file, argv[i]) == OPEN_FILE) {
                if (editorAddFileToActiveSplit(&file) == -1) {
                    break;
                }
            }
        }
    }

    gEditor.state = EDIT_MODE;
    if (gEditor.file_count == 0) {
        if (gEditor.explorer.node) {
            gEditor.state = EXPLORER_MODE;
        } else {
            editorNewUntitledFile(&file);
            editorAddFileToActiveSplit(&file);
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
