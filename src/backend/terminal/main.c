#include "editor/buildnum.h"
#include "editor/config.h"
#include "editor/console.h"
#include "editor/editor.h"
#include "editor/file_io.h"
#include "editor/row.h"

#include "editor/panels/edit.h"
#include "editor/panels/explorer.h"

#include "utils/opt.h"
#include "utils/os.h"

#include "os.h"
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
    bool readonly_mode = false;
    char* config_path = NULL;
    // TODO: Change this to VECTOR(Str)
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

        // TODO: Free args
        case 'v':
            printf("Exe version %s (%s)\n", EDITOR_VERSION, EDITOR_NAME);
            printf("Exe build: %s %s (%d)\n", editor_build_time,
                   editor_build_date, editorGetBuildNumber());
            return 0;

        case '?':
        case 'h':
            usage();
            return 0;
    }

    editorInit();

    // Load config
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

    // Post-config setup
    if (readonly_mode) {
        readonly.setInt(1);
    }

    if (gEditor.explorer_panel->base.layout->fixed_size !=
        ex_default_width.int_value) {
        gEditor.explorer_panel->base.layout->fixed_size =
            ex_default_width.int_value;
        layoutUpdate(gEditor.ui.root);
    }

    // Setup now for isStdinTty
    terminalOsInit();

    // Load files
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

    // Setup terminal to show loading
    terminalInit();
    terminalStart();

    if (!stdin_piped) {
        for (int i = 0; i < argc; i++) {
            EditorOpenStatus result = editorLoadFile(&file, argv[i], false);
            if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
                if (editorAddFileToActiveSplit(&file) == -1) {
                    break;
                }
            }
        }
    }

    argsFree(argc_utf8, argv_utf8);

    // Setup panel states
    if (gEditor.file_count == 0) {
        if (start_new_file.int_value && !gEditor.explorer_panel->node) {
            editorNewUntitledFile(&file);
            editorAddFileToActiveSplit(&file);
            uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.active_edit_panel);
        } else if (gEditor.explorer_panel->node) {
            uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.explorer_panel);
        } else {
            uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.welcome_panel);
        }
    } else {
        uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.active_edit_panel);
    }

    gEditor.state = STATE_RUNNING;

    // Main loop
    while (gEditor.state != STATE_EXIT) {
        terminalRefreshScreen(false);
        terminalProcessInput();
    }

    terminalExit();

#ifndef NDEBUG
    terminalFree();
    editorFree();
#endif

    return 0;
}
