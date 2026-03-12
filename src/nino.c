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

static char* copyArg(const char* arg) {
    size_t len = strlen(arg) + 1;
    char* copy = malloc_s(len);
    memcpy(copy, arg, len);
    return copy;
}

static void usage(void) {
    printf("Usage: " EDITOR_NAME " [options] [file...]\n");
    printf("Options:\n");
    printf("  -c <cmd>     Execute <cmd> after config\n");
    printf("  -u <file>    Use this config file\n");
    printf("  -R           Readonly mode\n");
    printf("  -v           Print version information and exit\n");
    printf("  -h           Print this help message and exit\n");
}

int main(int argc, char* argv[]) {
    editorInit();

    bool readonly_mode = false;
    char* config_path = NULL;
    size_t startup_cmd_count = 0;
    char** startup_cmds = NULL;

    int argc_utf8 = argc;
    char** argv_utf8 = argv;
    argsInit(&argc_utf8, &argv_utf8);
    argc = argc_utf8;
    argv = argv_utf8;
    FOR_OPTS(argc, argv) {
        case 'c': {
            const char* command = OPTARG(argc, argv);
            startup_cmds = realloc_s(startup_cmds,
                                     sizeof(char*) * (startup_cmd_count + 1));
            startup_cmds[startup_cmd_count++] = copyArg(command);
        } break;

        case 'u':
            free(config_path);
            config_path = copyArg(OPTARG(argc, argv));
            break;

        case 'R':
            readonly_mode = true;
            break;

        case 'v':
            printf("Exe version %s (%s)\n", EDITOR_VERSION, EDITOR_NAME);
            printf("Exe build: %s %s (%d)\n", editor_build_time,
                   editor_build_date, editorGetBuildNumber());
            goto DONE;

        case '?':
        case 'h':
            usage();
            goto DONE;
    }

    if (!config_path) {
        editorLoadInitConfig();
    } else if (strcmp(config_path, "NONE") != 0) {
        if (!editorLoadConfig(config_path)) {
            editorMsg("Failed to load config: %s", config_path);
        }
    }
    free(config_path);

    for (size_t i = 0; i < startup_cmd_count; i++) {
        editorCmd(startup_cmds[i]);
        free(startup_cmds[i]);
    }
    free(startup_cmds);

    if (readonly_mode) {
        CONVAR_SETINT(readonly, 1);
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
            OpenStatus result = editorLoadFile(&file, argv[i], false);
            if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
                if (editorAddFileToActiveSplit(&file) == -1) {
                    break;
                }
            }
        }
    }

    argsFree(argc_utf8, argv_utf8);

    if (gEditor.file_count == 0) {
        gEditor.state = STATE_EXPLORER;
        if (CONVAR_GETINT(start_new_file) && !gEditor.explorer.node) {
            editorNewUntitledFile(&file);
            editorAddFileToActiveSplit(&file);
            gEditor.state = STATE_EDIT;
        }
    } else {
        gEditor.state = STATE_EDIT;
    }

    gEditor.explorer.prefered_width = CONVAR_GETINT(ex_default_width);
    if (gEditor.explorer.node == NULL) {
        gEditor.explorer.width = 0;
    } else {
        gEditor.explorer.width = gEditor.explorer.prefered_width;
    }

    while (gEditor.state != STATE_EXIT) {
        editorRefreshScreen();
        editorProcessKeypress();
    }

DONE:
    terminalExit();
#ifndef NDEBUG
    editorFree();
#endif
    return 0;
}
