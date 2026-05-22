#include "config.h"

#include "buildnum.h"
#include "editor.h"
#include "os.h"
#include "prompt.h"
#include "terminal.h"

CCommand args;

static void cvarTabSizeCallback(void);
static void cvarSyntaxCallback(void);
static void cvarExplorerCallback(void);
static void cvarMouseCallback(void);

CONVAR(tabsize, "4", "Tab size.", true, 1, true, 16, cvarTabSizeCallback);
CONVAR(whitespace, "1", "Use whitespace instead of tab.");
CONVAR(autoindent, "0", "Enable auto indent.");
CONVAR(backspace, "1", "Use hungry backspace.");
CONVAR(bracket, "0", "Use auto bracket completion.");
CONVAR(trailing, "1", "Highlight trailing spaces.");
CONVAR(drawspace, "0", "Render whitespace and tab.");
CONVAR(syntax, "1", "Enable syntax highlight.", cvarSyntaxCallback);
CONVAR(helpinfo, "1", "Show the help information.");
CONVAR(intro, "1", "Show the introductory message when no files are open.");
CONVAR(start_new_file,
       "0",
       "Create an untitled file when starting with no files and no "
       "directory.");
CONVAR(ignorecase,
       "2",
       "Use case insensitive search. Set to 2 to use smart case.",
       true,
       0,
       true,
       2);
CONVAR(mouse, "1", "Enable mouse mode.", cvarMouseCallback);
CONVAR(osc52_copy, "1", "Copy to system clipboard using OSC52.");

CONVAR(shell, "", "Shell used by the run command. (full path)");

CONVAR(cmd_expand_depth,
       "1024",
       "Max depth for alias expansion.",
       true,
       0,
       false,
       0);

CONVAR(ex_default_width,
       "40",
       "File explorer default width.",
       true,
       0,
       false,
       0);
CONVAR(ex_show_hidden,
       "1",
       "Show hidden files in the file explorer.",
       cvarExplorerCallback);
CONVAR(newline_default,
       "0",
       "Set the default EOL sequence (LF/CRLF). 0 is OS default.");
CONVAR(ttimeoutlen,
       "50",
       "Time in milliseconds to wait for a key code sequence to complete.",
       true,
       0,
       false,
       0);
CONVAR(lineno, "1", "Show line numbers.");
CONVAR(readonly, "0", "Open files in read-only mode.");

static void reloadSyntax(void) {
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count == 0)
            continue;
        editorFileReloadHighlight(&gEditor.files[i]);
    }
}

static void reloadExplorer(void) {
    if (gEditor.explorer.node) {
        gEditor.explorer.node = editorExplorerCreate(".");
        gEditor.explorer.node->is_open = true;
        editorExplorerRefresh();

        gEditor.explorer.offset = 0;
        gEditor.explorer.selected_index = 0;
    }
}

static void cvarTabSizeCallback(void) {
    // Update rsize
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count == 0)
            continue;
        EditorFile* file = &gEditor.files[i];
        for (int j = 0; j < file->num_rows; j++) {
            file->row[j].rsize =
                editorRowCxToRx(&file->row[j], file->row[j].size);
        }
    }
}

static void cvarSyntaxCallback(void) {
    reloadSyntax();
}

static void cvarExplorerCallback(void) {
    reloadExplorer();
}

static void cvarMouseCallback(void) {
    bool mode = !!mouse.int_value;
    if (gEditor.mouse_mode != mode) {
        gEditor.mouse_mode = mode;
        if (gEditor.state != STATE_LOADING) {
            if (mode) {
                enableMouse();
            } else {
                disableMouse();
            }
        }
    }
}

const ColorElement color_element_map[UI_COLOR_COUNT] = {
    {"bg", &gEditor.color_cfg[UI_COLOR_BG]},

    {"top.fg", &gEditor.color_cfg[UI_COLOR_TOP_FG]},
    {"top.bg", &gEditor.color_cfg[UI_COLOR_TOP_BG]},
    {"top.tabs.fg", &gEditor.color_cfg[UI_COLOR_TOP_TABS_FG]},
    {"top.tabs.bg", &gEditor.color_cfg[UI_COLOR_TOP_TABS_BG]},
    {"top.select.fg", &gEditor.color_cfg[UI_COLOR_TOP_SELECT_FG]},
    {"top.select.bg", &gEditor.color_cfg[UI_COLOR_TOP_SELECT_BG]},

    {"explorer.bg", &gEditor.color_cfg[UI_COLOR_EXPLORER_BG]},
    {"explorer.select", &gEditor.color_cfg[UI_COLOR_EXPLORER_SELECT]},
    {"explorer.directory", &gEditor.color_cfg[UI_COLOR_EXPLORER_DIRECTORY]},
    {"explorer.file", &gEditor.color_cfg[UI_COLOR_EXPLORER_FILE]},
    {"explorer.focus", &gEditor.color_cfg[UI_COLOR_EXPLORER_FOCUS]},

    {"prompt.fg", &gEditor.color_cfg[UI_COLOR_PROMPT_FG]},
    {"prompt.bg", &gEditor.color_cfg[UI_COLOR_PROMPT_BG]},

    // TODO: Customizable status bar
    {"status.fg", &gEditor.color_cfg[UI_COLOR_STATUS_FG]},
    {"status.bg", &gEditor.color_cfg[UI_COLOR_STATUS_BG]},
    {"status.lang.fg", &gEditor.color_cfg[UI_COLOR_STATUS_LANG_FG]},
    {"status.lang.bg", &gEditor.color_cfg[UI_COLOR_STATUS_LANG_BG]},
    {"status.pos.fg", &gEditor.color_cfg[UI_COLOR_STATUS_POS_FG]},
    {"status.pos.bg", &gEditor.color_cfg[UI_COLOR_STATUS_POS_BG]},

    {"lineno.fg", &gEditor.color_cfg[UI_COLOR_LINENO_FG]},
    {"lineno.bg", &gEditor.color_cfg[UI_COLOR_LINENO_BG]},

    {"cursorline", &gEditor.color_cfg[UI_COLOR_CURSORLINE]},

    {"hl.normal", &gEditor.color_cfg[UI_COLOR_HL_NORMAL]},
    {"hl.comment", &gEditor.color_cfg[UI_COLOR_HL_COMMENT]},
    {"hl.keyword1", &gEditor.color_cfg[UI_COLOR_HL_KEYWORD1]},
    {"hl.keyword2", &gEditor.color_cfg[UI_COLOR_HL_KEYWORD2]},
    {"hl.keyword3", &gEditor.color_cfg[UI_COLOR_HL_KEYWORD3]},
    {"hl.string", &gEditor.color_cfg[UI_COLOR_HL_STRING]},
    {"hl.number", &gEditor.color_cfg[UI_COLOR_HL_NUMBER]},
    {"hl.trailing", &gEditor.color_cfg[UI_COLOR_HL_TRAILING]},

    {"hl.space", &gEditor.color_cfg[UI_COLOR_HL_SPACE]},
    {"hl.match", &gEditor.color_cfg[UI_COLOR_HL_MATCH]},
    {"hl.select", &gEditor.color_cfg[UI_COLOR_HL_SELECT]},
};

CON_COMMAND(color, "Change the color of an element.") {
    if (args.argc != 2 && args.argc != 3) {
        editorMsg("Usage: color <element> [color]");
        return;
    }

    Color* target = NULL;
    int element_num = sizeof(color_element_map) / sizeof(ColorElement);
    for (int i = 0; i < element_num; i++) {
        if (strcmp(color_element_map[i].label, args.argv[1]) == 0) {
            target = color_element_map[i].color;
            break;
        }
    }

    if (!target) {
        editorMsg("Unknown element \"%s\".", args.argv[1]);
        return;
    }

    if (args.argc == 2) {
        char buf[16];
        colorToStr(*target, buf);
        editorMsg("%s = %s", args.argv[1], buf);
    } else if (args.argc == 3) {
        if (!strToColor(args.argv[2], target)) {
            editorMsg("Invalid color string \"%s\".", args.argv[2]);
        }
    }
}

CON_COMMAND(exec, "Execute a config file.") {
    if (args.argc != 2) {
        editorMsg("Usage: exec <file>");
        return;
    }

    char filename[EDITOR_PATH_MAX] = {0};
    snprintf(filename, sizeof(filename), "%s", args.argv[1]);
    addDefaultExtension(filename, EDITOR_CONFIG_EXT, sizeof(filename));

    if (!editorLoadConfig(filename)) {
        // Try config directory
        const char* home_dir = getEnv(ENV_HOME);
        if (!home_dir) {
            editorMsg("exec: Failed to exec \"%s\"", args.argv[1]);
            return;
        }

        char config_path[EDITOR_PATH_MAX];
        int len = snprintf(config_path, sizeof(config_path),
                           PATH_CAT("%s", CONF_DIR, "%s"), home_dir, filename);

        if (len < 0 || !editorLoadConfig(config_path)) {
            editorMsg("exec: Failed to exec \"%s\"", args.argv[1]);
            return;
        }
    }
}

CON_COMMAND(lang, "Set the syntax highlighting language of the current file.") {
    if (args.argc != 2) {
        editorMsg("Usage: lang <name>");
        return;
    }

    if (gEditor.file_count == 0) {
        editorMsg("lang: No file opened");
        return;
    }

    EditorFile* file = editorGetActiveFile();

    const char* name = args.argv[1];
    for (EditorSyntax* s = gEditor.HLDB; s; s = s->next) {
        // Match the language name or the externaion
        if (strCaseCmp(name, s->file_type) == 0) {
            editorSetSyntaxHighlight(file, s);
            return;
        }

        for (size_t i = 0; i < s->file_exts.size; i++) {
            int is_ext = (s->file_exts.data[i][0] == '.');
            if ((is_ext && strCaseCmp(name, &s->file_exts.data[i][1]) == 0) ||
                (!is_ext && strCaseStr(name, s->file_exts.data[i]))) {
                editorSetSyntaxHighlight(file, s);
                return;
            }
        }
    }

    editorMsg("lang: \"%s\" not found", name);
}

CON_COMMAND(hldb_load, "Load a syntax highlighting JSON file.") {
    if (args.argc != 2) {
        editorMsg("Usage: hldb_load <json file>");
        return;
    }

    char filename[EDITOR_PATH_MAX] = {0};
    snprintf(filename, sizeof(filename), "%s", args.argv[1]);
    addDefaultExtension(filename, ".json", sizeof(filename));

    if (!editorLoadHLDB(filename)) {
        // Try config directory
        const char* home_dir = getEnv(ENV_HOME);
        if (!home_dir) {
            editorMsg("hldb_load: Failed to load \"%s\"", args.argv[1]);
            return;
        }

        char config_path[EDITOR_PATH_MAX];
        int len = snprintf(config_path, sizeof(config_path),
                           PATH_CAT("%s", CONF_DIR, "%s"), home_dir, filename);

        if (len < 0 || !editorLoadHLDB(config_path)) {
            editorMsg("hldb_load: Failed to load \"%s\"", args.argv[1]);
            return;
        }
    }

    reloadSyntax();
}

CON_COMMAND(hldb_reload_all, "Reload syntax highlighting database.") {
    UNUSED(args.argc);

    editorFreeHLDB();
    editorInitHLDB();
    reloadSyntax();
}

CON_COMMAND(newline, "Set the EOL sequence (LF/CRLF).") {
    if (gEditor.file_count == 0) {
        editorMsg("newline: No file opened");
        return;
    }

    EditorFile* file = editorGetActiveFile();

    if (args.argc == 1) {
        editorMsg("%s", (file->newline == NL_UNIX) ? "LF" : "CRLF");
        return;
    }

    int nl;

    if (strCaseCmp(args.argv[1], "lf") == 0) {
        nl = NL_UNIX;
    } else if (strCaseCmp(args.argv[1], "crlf") == 0) {
        nl = NL_DOS;
    } else {
        editorMsg("Usage: newline <LF/CRLF>");
        return;
    }

    if (file->read_only && !file->unlocked) {
        editorMsg("File is read-only.");
        return;
    }

    if (file->newline == nl) {
        return;
    }

    EditorAction* action = calloc_s(1, sizeof(EditorAction));
    action->type = ACTION_ATTRI;
    action->attri.new_newline = nl;
    action->attri.old_newline = file->newline;

    file->newline = nl;

    editorAppendAction(file, action);
}

CON_COMMAND(unlock, "Allow editing a read-only file.") {
    if (gEditor.file_count == 0) {
        editorMsg("unlock: No file opened");
        return;
    }

    EditorFile* file = editorGetActiveFile();
    if (!file->read_only) {
        editorMsg("File is not read-only.");
        return;
    }

    if (file->unlocked) {
        editorMsg("File already unlocked.");
        return;
    }

    file->unlocked = true;
    editorMsg("File unlocked. Note: file is still read-only on disk.");
}

CON_COMMAND(reload, "Reload the current file from disk.") {
    if (gEditor.file_count == 0) {
        editorMsg("reload: No file opened");
        return;
    }

    EditorTab* curr_tab = editorGetActiveTab();
    EditorFile* curr_file = editorTabGetFile(curr_tab);
    int file_index = curr_tab->file_index;

    if (curr_file->dirty) {
        editorMsg("File has unsaved changes.");
        return;
    }

    if (!curr_file->filename) {
        editorMsg("File is untitled.");
        return;
    }

    EditorFile temp_file;
    OpenStatus result = editorLoadFile(&temp_file, curr_file->filename, true);
    switch (result) {
        case OPEN_FILE: {
            int reference_count = curr_file->reference_count;
            editorFreeFile(curr_file);
            *curr_file = temp_file;
            curr_file->reference_count = reference_count;

            for (int i = 0; i < gEditor.split_count; i++) {
                EditorSplit* split = &gEditor.splits[i];
                for (int j = 0; j < split->tab_count; j++) {
                    EditorTab* tab = &split->tabs[j];
                    // TODO: Move cursor based on the position in the old
                    // file
                    if (tab->file_index == file_index) {
                        tab->cursor.x = 0;
                        tab->cursor.y = 0;
                        tab->cursor.is_selected = false;
                        tab->sx = 0;
                        tab->row_offset = 0;
                        tab->col_offset = 0;
                    }
                }
            }

            editorMsg("File reloaded from disk.");
        } break;

        case OPEN_FILE_NEW:
            editorFreeFile(&temp_file);
            editorMsg("File does not exist on disk.");
            break;

        case OPEN_DIR:
            editorMsg("File is now a directory on disk.");
            break;

        default:
            editorMsg("Failed to reload file.");
    }
}

int editorGetDefaultNewline(void) {
    int nl = NL_DEFAULT;
    const char* option = newline_default.string_value;
    if (strCaseCmp(option, "lf") == 0) {
        nl = NL_UNIX;
    } else if (strCaseCmp(option, "crlf") == 0) {
        nl = NL_DOS;
    }
    return nl;
}

CON_COMMAND(echo, "Echo text to console.") {
    if (args.argc < 2)
        return;

    int total_len = 0;
    char buf[COMMAND_MAX_LENGTH];

    for (int i = 1; i < args.argc; i++) {
        int arg_len = strlen(args.argv[i]);
        int space_len = (i > 1) ? 1 : 0;
        if (total_len + space_len + arg_len + 1 <= COMMAND_MAX_LENGTH) {
            if (space_len) {
                buf[total_len++] = ' ';
            }
            memcpy(&buf[total_len], args.argv[i], arg_len);
            total_len += arg_len;
        } else {
            break;
        }
    }
    buf[total_len] = '\0';
    editorMsg("%s", buf);
}

CON_COMMAND(clear, "Clear all console output.") {
    UNUSED(args.argc);
    editorMsgClear();
}

CON_COMMAND(version, "Print version info string.") {
    UNUSED(args.argc);

    editorMsg("Exe version %s (%s)", EDITOR_VERSION, EDITOR_NAME);
    editorMsg("Exe build: %s %s (%d)", editor_build_time, editor_build_date,
              editorGetBuildNumber());
}

static void showCmdHelp(const ConCommandBase* cmd) {
    if (cmd->is_command) {
        editorMsg("\"%s\"", cmd->name);
    } else {
        const ConVar* cvar = (ConVar*)cmd;
        if (strcmp(cvar->default_string, cvar->string_value) == 0) {
            editorMsg("\"%s\" = \"%s\"", cvar->name, cvar->string_value);
        } else {
            editorMsg("\"%s\" = \"%s\" (def. \"%s\" )", cvar->name,
                      cvar->string_value, cvar->default_string);
        }

        if (cvar->has_min)
            editorMsg(" min: %d", cvar->min_value);
        if (cvar->has_max)
            editorMsg(" max: %d", cvar->max_value);
    }
    editorMsg(" - %s", cmd->help_string);
}

CON_COMMAND(help, "Find help about a convar/concommand.") {
    if (args.argc != 2) {
        editorMsg("Usage: help <command>");
        return;
    }
    ConCommandBase* cmd = editorFindCmd(args.argv[1]);
    if (!cmd) {
        editorMsg("help: No cvar or command named \"%s\".", args.argv[1]);
        return;
    }
    showCmdHelp(cmd);
}

CON_COMMAND(
    find,
    "Find concommands with the specified string in their name/help text.") {
    if (args.argc != 2) {
        editorMsg("Usage: find <string>");
        return;
    }

    const char* s = args.argv[1];

    ConCommandBase* curr = gEditor.cvars;
    while (curr) {
        if (strCaseStr(curr->name, s) || strCaseStr(curr->help_string, s)) {
            showCmdHelp(curr);
        }
        curr = curr->next;
    }
}

CON_COMMAND(suspend, "Suspend the editor (Not available on Windows).") {
    osSuspend();
}

CON_COMMAND(run, "Run a shell command.") {
    if (args.argc < 2) {
        editorMsg("Usage: run <command>");
        return;
    }

    abuf cmd = ABUF_INIT;
    for (int i = 1; i < args.argc; i++) {
        if (i > 1)
            abufAppendN(&cmd, " ", 1);
        abufAppendN(&cmd, args.argv[i], strlen(args.argv[i]));
    }
    abufAppendN(&cmd, "\0", 1);

    osRunShell(shell.string_value, cmd.buf);

    abufFree(&cmd);
}

CON_COMMAND(quit, "Exit the editor (without save).") {
    gEditor.state = STATE_EXIT;
}

#ifndef NDEBUG

CON_COMMAND(crash, "Cause the editor to crash. (Debug!!)") {
    int crash_type = 0;
    if (args.argc > 1) {
        strToInt(args.argv[1], &crash_type);
    }

    switch (crash_type) {
        case 0:
            // SIGSEGV
            *(volatile char*)0 = 0;
            break;
        case 1:
            // SIGABRT
            abort();
        default:
            editorMsg("Unknown crash type.");
    }
}

#endif

const Color color_default[UI_COLOR_COUNT] = {
    [UI_COLOR_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 30, .g = 30, .b = 30}},
        },

    [UI_COLOR_TOP_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 229, .g = 229, .b = 229}},
        },
    [UI_COLOR_TOP_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 37, .g = 37, .b = 37}},
        },
    [UI_COLOR_TOP_TABS_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 150, .g = 150, .b = 150}},
        },
    [UI_COLOR_TOP_TABS_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 45, .g = 45, .b = 45}},
        },
    [UI_COLOR_TOP_SELECT_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 229, .g = 229, .b = 229}},
        },
    [UI_COLOR_TOP_SELECT_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 87, .g = 80, .b = 104}},
        },

    [UI_COLOR_EXPLORER_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 37, .g = 37, .b = 37}},
        },
    [UI_COLOR_EXPLORER_SELECT] =
        {
            .kind = COLOR_RGB,
            {{.r = 87, .g = 80, .b = 104}},
        },
    [UI_COLOR_EXPLORER_DIRECTORY] =
        {
            .kind = COLOR_RGB,
            {{.r = 236, .g = 193, .b = 132}},
        },
    [UI_COLOR_EXPLORER_FILE] =
        {
            .kind = COLOR_RGB,
            {{.r = 229, .g = 229, .b = 229}},
        },
    [UI_COLOR_EXPLORER_FOCUS] =
        {
            .kind = COLOR_RGB,
            {{.r = 45, .g = 45, .b = 45}},
        },

    [UI_COLOR_PROMPT_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 229, .g = 229, .b = 229}},
        },
    [UI_COLOR_PROMPT_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 60, .g = 60, .b = 60}},
        },

    [UI_COLOR_STATUS_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 225, .g = 219, .b = 239}},
        },
    [UI_COLOR_STATUS_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 87, .g = 80, .b = 104}},
        },
    [UI_COLOR_STATUS_LANG_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 225, .g = 219, .b = 239}},
        },
    [UI_COLOR_STATUS_LANG_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 169, .g = 107, .b = 33}},
        },
    [UI_COLOR_STATUS_POS_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 225, .g = 219, .b = 239}},
        },
    [UI_COLOR_STATUS_POS_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 217, .g = 138, .b = 43}},
        },

    [UI_COLOR_LINENO_FG] =
        {
            .kind = COLOR_RGB,
            {{.r = 127, .g = 127, .b = 127}},
        },
    [UI_COLOR_LINENO_BG] =
        {
            .kind = COLOR_RGB,
            {{.r = 30, .g = 30, .b = 30}},
        },

    [UI_COLOR_CURSORLINE] =
        {
            .kind = COLOR_RGB,
            {{.r = 40, .g = 40, .b = 40}},
        },

    [UI_COLOR_HL_NORMAL] =
        {
            .kind = COLOR_RGB,
            {{.r = 229, .g = 229, .b = 229}},
        },
    [UI_COLOR_HL_COMMENT] =
        {
            .kind = COLOR_RGB,
            {{.r = 106, .g = 153, .b = 85}},
        },
    [UI_COLOR_HL_KEYWORD1] =
        {
            .kind = COLOR_RGB,
            {{.r = 197, .g = 134, .b = 192}},
        },
    [UI_COLOR_HL_KEYWORD2] =
        {
            .kind = COLOR_RGB,
            {{.r = 86, .g = 156, .b = 214}},
        },
    [UI_COLOR_HL_KEYWORD3] =
        {
            .kind = COLOR_RGB,
            {{.r = 78, .g = 201, .b = 176}},
        },
    [UI_COLOR_HL_STRING] =
        {
            .kind = COLOR_RGB,
            {{.r = 206, .g = 145, .b = 120}},
        },
    [UI_COLOR_HL_NUMBER] =
        {
            .kind = COLOR_RGB,
            {{.r = 181, .g = 206, .b = 168}},
        },
    [UI_COLOR_HL_TRAILING] =
        {
            .kind = COLOR_RGB,
            {{.r = 255, .g = 100, .b = 100}},
        },

    [UI_COLOR_HL_SPACE] =
        {
            .kind = COLOR_RGB,
            {{.r = 63, .g = 63, .b = 63}},
        },
    [UI_COLOR_HL_MATCH] =
        {
            .kind = COLOR_RGB,
            {{.r = 89, .g = 46, .b = 20}},
        },
    [UI_COLOR_HL_SELECT] =
        {
            .kind = COLOR_RGB,
            {{.r = 38, .g = 79, .b = 120}},
        },
};

#define MAX_ALIAS_NAME 32

typedef struct CmdAlias CmdAlias;
struct CmdAlias {
    CmdAlias* next;
    char name[MAX_ALIAS_NAME];
    char* value;
};

static CmdAlias* cmd_alias = NULL;

static CmdAlias* findAlias(const char* name) {
    CmdAlias* a = cmd_alias;
    while (a) {
        if (strCaseCmp(name, a->name) == 0) {
            break;
        }
        a = a->next;
    }

    return a;
}

CON_COMMAND(alias, "Alias a command.") {
    if (args.argc < 2) {
        editorMsg("Usage: alias <name> [value]");
        return;
    }

    char* s = args.argv[1];
    if (strlen(s) >= MAX_ALIAS_NAME) {
        editorMsg("Alias name is too long");
        return;
    }

    // If the alias already exists, reuse it
    CmdAlias* a = findAlias(s);
    if (args.argc == 2) {
        if (!a) {
            editorMsg("\"%s\" is not aliased", s);
        } else {
            editorMsg("\"%s\" = \"%s\"", s, a->value);
        }
        return;
    }

    if (!a) {
        a = malloc_s(sizeof(CmdAlias));
        a->next = cmd_alias;
        cmd_alias = a;
    } else {
        free(a->value);
    }

    strcpy(a->name, s);

    // Copy the rest of the command line
    int total_len = 0;
    char cmd[COMMAND_MAX_LENGTH];

    for (int i = 2; i < args.argc; i++) {
        int arg_len = strlen(args.argv[i]);
        int space_len = (i > 2) ? 1 : 0;
        if (total_len + space_len + arg_len + 1 <= COMMAND_MAX_LENGTH) {
            if (space_len) {
                cmd[total_len++] = ' ';
            }
            memcpy(&cmd[total_len], args.argv[i], arg_len);
            total_len += arg_len;
        } else {
            break;
        }
    }
    cmd[total_len] = '\0';

    size_t size = total_len + 1;
    a->value = malloc_s(size);
    snprintf(a->value, size, "%s", cmd);
}

CON_COMMAND(unalias, "Remove an alias.") {
    if (args.argc != 2) {
        editorMsg("Usage: unalias <name>");
        return;
    }

    const char* s = args.argv[1];

    if (!cmd_alias) {
        editorMsg("%s not found", s);
        return;
    }

    if (strCaseCmp(cmd_alias->name, s) == 0) {
        CmdAlias* temp = cmd_alias;
        cmd_alias = cmd_alias->next;
        free(temp->value);
        free(temp);
        return;
    }

    CmdAlias* a = cmd_alias;
    while (a->next && strCaseCmp(a->next->name, s) != 0) {
        a = a->next;
    }

    if (!a->next) {
        editorMsg("%s not found", s);
        return;
    }

    CmdAlias* temp = a->next;
    a->next = a->next->next;
    free(temp->value);
    free(temp);
}

static void parseLine(const char* cmd, int depth);

static void cvarCmdCallback(ConVar* cvar) {
    if (args.argc < 2) {
        showCmdHelp((ConCommandBase*)cvar);
        return;
    }
    editorSetConVar(cvar, args.argv[1], true);
}

static void executeCommand(int depth) {
    if (args.argc < 1)
        return;

    if (args.argc > COMMAND_MAX_ARGC) {
        editorMsg("Command overflows the argument buffer. Clamped!");
        args.argc = COMMAND_MAX_ARGC;
    }

    CmdAlias* a = findAlias(args.argv[0]);
    if (a) {
        parseLine(a->value, depth + 1);
        return;
    }

    ConCommandBase* cmd = editorFindCmd(args.argv[0]);
    if (!cmd) {
        editorMsg("Unknown command \"%s\".", args.argv[0]);
        return;
    }

    if (cmd->is_command) {
        ((ConCommand*)cmd)->callback();
    } else {
        cvarCmdCallback((ConVar*)cmd);
    }
}

static void resetArgs(void) {
    for (int i = 0; i < args.argc; i++) {
        free(args.argv[i]);
    }
    args.argc = 0;
}

static void parseLine(const char* cmd, int depth) {
    if (depth > cmd_expand_depth.int_value) {
        editorMsg("Reached max alias expansion depth.");
        return;
    }

    // Command line parsing
    resetArgs();
    while (*cmd != '\0' && *cmd != '#') {
        switch (*cmd) {
            case '\t':
            case ' ':
                cmd++;
                break;

            case ';':
                executeCommand(depth);
                resetArgs();
                cmd++;
                break;

            default: {
                bool in_quote = (*cmd == '"');
                if (in_quote) {
                    cmd++;
                }

                char* buf = calloc_s(sizeof(char), COMMAND_MAX_LENGTH);
                int buf_len = 0;

                while (*cmd != '\0' &&
                       (in_quote
                            ? (*cmd != '"')
                            : (*cmd != '#' && *cmd != ';' && *cmd != ' '))) {
                    if (buf_len < COMMAND_MAX_LENGTH - 1) {
                        buf[buf_len] = *cmd;
                    }
                    buf_len++;
                    cmd++;
                }

                if (in_quote && *cmd == '"') {
                    cmd++;
                }

                if (args.argc < COMMAND_MAX_ARGC) {
                    args.argv[args.argc] = buf;
                } else {
                    free(buf);
                }

                // Let argc go pass COMMAND_MAX_ARGC.
                // We will detect it in executeCommand.
                args.argc++;
            }
        }
    }

    executeCommand(depth);
}

void editorRegisterCommands(void) {
    editorInitConVar(&tabsize);
    editorInitConVar(&whitespace);
    editorInitConVar(&autoindent);
    editorInitConVar(&backspace);
    editorInitConVar(&bracket);
    editorInitConVar(&trailing);
    editorInitConVar(&drawspace);
    editorInitConVar(&syntax);
    editorInitConVar(&helpinfo);
    editorInitConVar(&intro);
    editorInitConVar(&start_new_file);
    editorInitConVar(&ignorecase);
    editorInitConVar(&mouse);
    editorInitConVar(&osc52_copy);
    editorInitConVar(&ex_default_width);
    editorInitConVar(&ex_show_hidden);
    editorInitConVar(&newline_default);
    editorInitConVar(&ttimeoutlen);
    editorInitConVar(&lineno);
    editorInitConVar(&readonly);

    editorInitConCommand(&color);
    editorInitConCommand(&lang);
    editorInitConCommand(&hldb_load);
    editorInitConCommand(&hldb_reload_all);
    editorInitConCommand(&newline);
    editorInitConCommand(&unlock);
    editorInitConCommand(&reload);

    editorInitConVar(&cmd_expand_depth);
    editorInitConCommand(&alias);
    editorInitConCommand(&unalias);
    editorInitConCommand(&exec);
    editorInitConCommand(&echo);
    editorInitConCommand(&clear);
    editorInitConCommand(&help);
    editorInitConCommand(&find);
    editorInitConCommand(&version);

    editorInitConVar(&shell);
    editorInitConCommand(&run);
    editorInitConCommand(&suspend);

    editorInitConCommand(&quit);

#ifndef NDEBUG
    editorInitConCommand(&crash);
#endif
}

void editorUnregisterCommands(void) {
    CmdAlias* a = cmd_alias;

    while (a) {
        CmdAlias* temp = a;
        a = a->next;
        free(temp->value);
        free(temp);
    }

    resetArgs();
}

bool editorLoadConfig(const char* path) {
    FILE* fp = openFile(path, "r");
    if (!fp)
        return false;

    char buf[COMMAND_MAX_LENGTH] = {0};
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        editorCmd(buf);
    }
    fclose(fp);
    return true;
}

void editorLoadInitConfig(void) {
    char path[EDITOR_PATH_MAX] = {0};
    const char* home_dir = getEnv(ENV_HOME);
    if (!home_dir)
        return;

    snprintf(path, sizeof(path), PATH_CAT("%s", CONF_DIR, EDITOR_RC_FILE),
             home_dir);
    if (!editorLoadConfig(path)) {
        snprintf(path, sizeof(path), PATH_CAT("%s", "." EDITOR_RC_FILE),
                 home_dir);
        editorLoadConfig(path);
    }
}

void editorCmd(const char* command) {
    parseLine(command, 0);
}

void editorOpenConfigPrompt(void) {
    char* query = editorPrompt("Prompt: ", STATE_CONFIG_PROMPT, NULL);
    if (query == NULL)
        return;

    editorMsg("] %s", query);
    editorCmd(query);

    free(query);
}

void editorSetConVar(ConVar* thisptr, const char* s, bool trigger_cb) {
    int n = 0;
    if (strToInt(s, &n) || thisptr->has_min || thisptr->has_max) {
        editorSetConVarInt(thisptr, n, trigger_cb);
        return;
    }

    strncpy(thisptr->string_value, s, COMMAND_MAX_LENGTH - 1);
    thisptr->string_value[COMMAND_MAX_LENGTH - 1] = '\0';
    thisptr->int_value = 0;

    if (trigger_cb && thisptr->change_callback) {
        thisptr->change_callback();
    }
}

void editorSetConVarInt(ConVar* thisptr, int n, bool trigger_cb) {
    if (thisptr->has_min && n < thisptr->min_value)
        n = thisptr->min_value;
    if (thisptr->has_max && n > thisptr->max_value)
        n = thisptr->max_value;

    snprintf(thisptr->string_value, sizeof(thisptr->string_value), "%d", n);
    thisptr->int_value = n;

    if (trigger_cb && thisptr->change_callback) {
        thisptr->change_callback();
    }
}

static inline void registerConCommandBase(ConCommandBase* thisptr) {
    thisptr->next = gEditor.cvars;
    gEditor.cvars = thisptr;
}

void editorInitConCommand(ConCommand* thisptr) {
    thisptr->is_command = true;
    registerConCommandBase((ConCommandBase*)thisptr);
}

void editorInitConVar(ConVar* thisptr) {
    thisptr->is_command = false;
    if (!thisptr->default_string)
        thisptr->default_string = "";
    editorSetConVar(thisptr, thisptr->default_string, false);
    registerConCommandBase((ConCommandBase*)thisptr);
}

ConCommandBase* editorFindCmd(const char* name) {
    ConCommandBase* result = NULL;
    ConCommandBase* curr = gEditor.cvars;
    while (curr) {
        if (strCaseCmp(name, curr->name) == 0) {
            result = curr;
            break;
        }
        curr = curr->next;
    }
    return result;
}

int editorGetLinenoWidth(const EditorFile* file) {
    return (lineno.int_value ? file->lineno_width : 0);
}
