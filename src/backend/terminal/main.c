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
    // clang-format off
    printf(
        "Usage: " EDITOR_NAME " [options] [file...]\n"
        "Options:\n"
        "  -c <cmd>                      Execute <cmd> after config\n"
        "  -u <file>                     Use this config file (\"NONE\" for no config)\n"
        "  -g <file:line{:character}>    Open file at specific position\n"
        "  -R                            Readonly mode\n"
        "  -v                            Print version information and exit\n"
        "  -h                            Print this help message and exit\n"
    );
    // clang-format on
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
    Vec(const char*) goto_files = {0};

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

            case 'g': {
                const char* filename = optArg(&parser);
                if (!filename)
                    missingArg(flag);
                vecPush(&goto_files, filename);
            } break;

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
            editorMsg("Failed to load config \"%s\".", config_path);
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
        // Goto files
        Str temp_filename = {0};
        for (size_t i = 0; i < goto_files.size; i++) {
            strClear(&temp_filename);

            FilenamePos filename_pos =
                parseFilenamePos(svFromCStr(goto_files.data[i]));
            strAppend(&temp_filename, filename_pos.filename);
            const char* filename = strGetCStr(&temp_filename);

            if (getFileType(filename) == FT_DIR) {
                editorMsg("\"%s\" is a directory but used in -g flag.",
                          filename);
                continue;
            }

            EditorOpenStatus result = editorLoadFile(&file, filename, false);
            if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
                if (editorAddFileToActiveSplit(&file) == -1) {
                    break;
                }
                editorScrollTo(gEditor.active_edit_panel, filename_pos.y - 1,
                               filename_pos.x - 1);
            }
        }
        strFree(&temp_filename);

        // Remaining
        for (size_t i = 0; i < file_argc; i++) {
            EditorOpenStatus result =
                editorLoadFile(&file, strGetCStr(&file_args[i]), false);
            if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
                if (editorAddFileToActiveSplit(&file) == -1) {
                    break;
                }
            }
        }
    }
    vecFree(&goto_files);

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
