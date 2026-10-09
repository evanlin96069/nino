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
#include "utils/str.h"
#include "utils/vec.h"

#include "os.h"
#include "terminal.h"

static void usage(void) {
    printf("Usage: " EDITOR_NAME " [options] [file...]\n");
    printf("Options:\n");
    printf("  -c <cmd>     Execute <cmd> after config\n");
    printf("  -u <file>    Use this config file\n");
    printf("  -R           Readonly mode\n");
    printf("  -v           Print version information and exit\n");
    printf("  -h           Print this help message and exit\n");
}

static void unknownArg(int flag) {
    fprintf(stderr, EDITOR_NAME ": unknown argument: -%c\n", flag);
    fprintf(stderr, "More info with: " EDITOR_NAME " -h\n");
    exit(1);
}

static void missingArg(int flag) {
    fprintf(stderr, EDITOR_NAME ": argument to '-%c' is missing\n", flag);
    fprintf(stderr, "More info with: " EDITOR_NAME " -h\n");
    exit(1);
}

int main(int argc, char* argv[]) {
    bool readonly_mode = false;
    const char* config_path = NULL;
    Vec(const char*) startup_cmds = {0};

    VecStr utf8_args = getUTF8Args(argc, argv);
    OptParser parser = optInit(&utf8_args);

    int flag;
    while ((flag = optNext(&parser))) {
        switch (flag) {
            case 'c': {
                const char* cmd = optArg(&parser);
                if (!cmd)
                    missingArg(flag);
                vecPush(&startup_cmds, cmd);
            } break;

            case 'u':
                config_path = optArg(&parser);
                if (!config_path)
                    missingArg(flag);
                break;

            case 'R':
                readonly_mode = true;
                break;

            case 'v':
                printf("Exe version %s (%s)\n", EDITOR_VERSION, EDITOR_NAME);
                printf("Exe build: %s %s (%d)\n", editor_build_time,
                       editor_build_date, editorGetBuildNumber());
                return 0;

            case '?':
            case 'h':
                usage();
                return 0;

            default:
                unknownArg(flag);
        }
    }

    size_t file_argc;
    const Str* file_args = optRemaining(&parser, &file_argc);

    editorInit();

    // Load config
    if (!config_path) {
        editorLoadInitConfig();
    } else if (strcmp(config_path, "NONE") != 0) {
        if (!editorLoadConfig(config_path)) {
            editorMsg("Failed to load config: %s", config_path);
        }
    }

    for (size_t i = 0; i < startup_cmds.size; i++) {
        editorCmd(startup_cmds.data[i]);
    }
    vecFree(&startup_cmds);

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
    if ((file_argc == 0 && !is_tty) ||
        (file_argc == 1 && svEql(svFromStr(file_args[0]), svFromCStr("-")))) {
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
        for (size_t i = 0; i < file_argc; i++) {
            // TODO: Current strGetCStr is not const and might invalidate data
            // if re-allocate (this won't actually happen because getUTF8Args
            // will return null-terminated)
            EditorOpenStatus result =
                editorLoadFile(&file, strGetCStr((Str*)&file_args[i]), false);
            if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
                if (editorAddFileToActiveSplit(&file) == -1) {
                    break;
                }
            }
        }
    }

    for (size_t i = 0; i < utf8_args.size; i++) {
        strFree(&utf8_args.data[i]);
    }
    vecFree(&utf8_args);

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
