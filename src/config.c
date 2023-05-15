#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "input.h"
#include "status.h"
#include "terminal.h"

static void cvarSyntaxCallback(void);
static void cvarMouseCallback(void);

CONVAR(tabsize, "Tab size.", "4", NULL);
CONVAR(whitespace, "Use whitespace instead of tab.", "1", NULL);
CONVAR(autoindent, "Enable auto indent.", "0", NULL);
CONVAR(backspace, "Use hungry backspace.", "1", NULL);
CONVAR(bracket, "Use auto bracket completion.", "0", NULL);
CONVAR(trailing, "Highlight trailing spaces.", "1", cvarSyntaxCallback);
CONVAR(syntax, "Enable syntax highlight.", "1", cvarSyntaxCallback);
CONVAR(helpinfo, "Show the help information.", "1", NULL);
CONVAR(ignorecase, "Use case insensitive search. Set to 2 to use smartcase.",
       "2", NULL);
CONVAR(mouse, "Enable mouse mode.", "1", cvarMouseCallback);

static void cvarSyntaxCallback(void) {
    // Reload all
    for (int i = 0; i < gEditor.file_count; i++) {
        for (int j = 0; j < gEditor.files[i].num_rows; j++) {
            editorUpdateRow(&gEditor.files[i], &gEditor.files[i].row[j]);
        }
    }
}

static void cvarMouseCallback(void) {
    bool mode = CONVAR_GETINT(mouse);
    if (gEditor.mouse_mode != mode) {
        if (mode)
            enableMouse();
        else
            disableMouse();
    }
}

typedef struct {
    const char* label;
    Color* color;
} ColorElement;

static const ColorElement color_element_map[] = {
    {"bg", &gEditor.color_cfg.bg},

    {"top.fg", &gEditor.color_cfg.top_status[0]},
    {"top.bg", &gEditor.color_cfg.top_status[1]},
    {"top.tabs.fg", &gEditor.color_cfg.top_status[2]},
    {"top.tabs.bg", &gEditor.color_cfg.top_status[3]},
    {"top.select.fg", &gEditor.color_cfg.top_status[4]},
    {"top.select.bg", &gEditor.color_cfg.top_status[5]},

    {"explorer.bg", &gEditor.color_cfg.explorer[0]},
    {"explorer.select", &gEditor.color_cfg.explorer[1]},
    {"explorer.directory", &gEditor.color_cfg.explorer[2]},
    {"explorer.file", &gEditor.color_cfg.explorer[3]},
    {"explorer.focus", &gEditor.color_cfg.explorer[4]},

    {"prompt.fg", &gEditor.color_cfg.prompt[0]},
    {"prompt.bg", &gEditor.color_cfg.prompt[1]},

    // TODO: Customizable status bar
    {"status.fg", &gEditor.color_cfg.status[0]},
    {"status.bg", &gEditor.color_cfg.status[1]},
    {"status.lang.fg", &gEditor.color_cfg.status[2]},
    {"status.lang.bg", &gEditor.color_cfg.status[3]},
    {"status.pos.fg", &gEditor.color_cfg.status[4]},
    {"status.pos.bg", &gEditor.color_cfg.status[5]},

    {"lineno.fg", &gEditor.color_cfg.line_number[0]},
    {"lineno.bg", &gEditor.color_cfg.line_number[1]},

    {"cursorline", &gEditor.color_cfg.cursor_line},

    {"hl.normal", &gEditor.color_cfg.highlight[HL_NORMAL]},
    {"hl.comment", &gEditor.color_cfg.highlight[HL_COMMENT]},
    {"hl.keyword1", &gEditor.color_cfg.highlight[HL_KEYWORD1]},
    {"hl.keyword2", &gEditor.color_cfg.highlight[HL_KEYWORD2]},
    {"hl.keyword3", &gEditor.color_cfg.highlight[HL_KEYWORD3]},
    {"hl.string", &gEditor.color_cfg.highlight[HL_STRING]},
    {"hl.number", &gEditor.color_cfg.highlight[HL_NUMBER]},
    {"hl.match", &gEditor.color_cfg.highlight[HL_MATCH]},
    {"hl.select", &gEditor.color_cfg.highlight[HL_SELECT]},
    {"hl.space", &gEditor.color_cfg.highlight[HL_SPACE]},
};

CON_COMMAND(color, "Change the color of an element.") {
    if (args.argc != 2 && args.argc != 3) {
        editorSetStatusMsg("Usage: color <element> [color]");
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
        editorSetStatusMsg("Unknown element \"%s\".", args.argv[1]);
        return;
    }

    if (args.argc == 2) {
        char buf[8];
        colorToStr(*target, buf);
        editorSetStatusMsg("%s = %s", args.argv[1], buf);
    } else if (args.argc == 3) {
        *target = strToColor(args.argv[2]);
    }
}

CON_COMMAND(help, "Find help about a convar/concommand.") {
    if (args.argc != 2) {
        editorSetStatusMsg("Usage: help <command>");
        return;
    }
    EditorConCmd* cmd = editorFindCmd(args.argv[1]);
    if (!cmd) {
        editorSetStatusMsg("help: No cvar or command named \"%s\".",
                           args.argv[1]);
        return;
    }
    editorSetStatusMsg("\"%s\" - %s", cmd->name, cmd->help_string);
}

const EditorColorScheme color_default = {
    .bg = {30, 30, 30},
    .top_status =
        {
            {229, 229, 229},
            {37, 37, 37},
            {150, 150, 150},
            {45, 45, 45},
            {229, 229, 229},
            {87, 80, 104},
        },
    .explorer =
        {
            {37, 37, 37},
            {87, 80, 104},
            {236, 193, 132},
            {229, 229, 229},
            {45, 45, 45},
        },
    .prompt =
        {
            {229, 229, 229},
            {60, 60, 60},
        },
    .status =
        {
            {225, 219, 239},
            {87, 80, 104},
            {225, 219, 239},
            {169, 107, 33},
            {225, 219, 239},
            {217, 138, 43},
        },
    .line_number =
        {
            {127, 127, 127},
            {30, 30, 30},
        },
    .cursor_line = {40, 40, 40},
    .highlight =
        {
            {229, 229, 229},
            {106, 153, 85},
            {197, 134, 192},
            {86, 156, 214},
            {78, 201, 176},
            {206, 145, 120},
            {181, 206, 168},
            {89, 46, 20},
            {38, 79, 120},
            {255, 100, 100},
        },
};

static void ConVarCmdCallback(EditorConCmd* thisptr, EditorConCmdArgs args) {
    if (args.argc < 2) {
        editorSetStatusMsg("%s = %s - %s", thisptr->name,
                           thisptr->cvar.string_val, thisptr->help_string);
        return;
    }
    editorSetConVar(&thisptr->cvar, args.argv[1]);
}

static void parseLine(char* line) {
    // remove comment
    char* hash = strchr(line, '#');
    if (hash)
        *hash = '\0';

    char* token = strtok(line, " ");
    EditorConCmdArgs args = {.argc = 0};
    for (int i = 0; token && i < 4; i++, args.argc++) {
        strcpy(args.argv[i], token);
        token = strtok(NULL, " ");
    }

    if (args.argc < 1)
        return;

    EditorConCmd* cmd = editorFindCmd(args.argv[0]);
    if (!cmd) {
        editorSetStatusMsg("Unknown command \"%s\".", args.argv[0]);
        return;
    }

    if (cmd->has_callback)
        cmd->callback(args);
    else
        ConVarCmdCallback(cmd, args);
}

void editorInitCommands() {
    INIT_CONVAR(tabsize);
    INIT_CONVAR(whitespace);
    INIT_CONVAR(autoindent);
    INIT_CONVAR(backspace);
    INIT_CONVAR(bracket);
    INIT_CONVAR(trailing);
    INIT_CONVAR(syntax);
    INIT_CONVAR(helpinfo);
    INIT_CONVAR(ignorecase);
    INIT_CONVAR(mouse);

    INIT_CONCOMMAND(color);
    INIT_CONCOMMAND(help);
}

void editorLoadConfig() {
    char path[256] = {0};
    strncpy(path, getenv("HOME"), sizeof(path) - 1);
    strncat(path, "/.ninorc", sizeof(path) - 1);
    FILE* fp = fopen(path, "r");
    if (!fp)
        return;

    char buf[128] = {0};
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        parseLine(buf);
    }
    fclose(fp);
}

void editorSetting() {
    char* query = editorPrompt("Config: %s", SETTING_MODE, NULL);
    if (query == NULL)
        return;

    parseLine(query);
    free(query);
}

void editorSetConVar(EditorConVar* thisptr, const char* string_val) {
    strncpy(thisptr->string_val, string_val, COMMAND_MAX_LENGTH);
    thisptr->string_val[COMMAND_MAX_LENGTH - 1] = '\0';
    thisptr->int_val = atoi(string_val);

    if (thisptr->callback) {
        thisptr->callback();
    }
}

static void registerConCmd(EditorConCmd* thisptr) {
    EditorConCmd* curr = gEditor.cvars;
    if (!curr) {
        gEditor.cvars = thisptr;
        return;
    }

    while (curr->next) {
        curr = curr->next;
    }

    curr->next = thisptr;
}

void editorInitConCmd(EditorConCmd* thisptr) {
    thisptr->next = NULL;
    thisptr->has_callback = true;
    registerConCmd(thisptr);
}

void editorInitConVar(EditorConCmd* thisptr) {
    thisptr->next = NULL;
    thisptr->has_callback = false;
    if (!thisptr->cvar.default_string)
        thisptr->cvar.default_string = "";
    editorSetConVar(&thisptr->cvar, thisptr->cvar.default_string);
    registerConCmd(thisptr);
}

EditorConCmd* editorFindCmd(const char* name) {
    EditorConCmd* result = NULL;
    EditorConCmd* curr = gEditor.cvars;
    while (curr) {
        if (strcmp(name, curr->name) == 0) {
            result = curr;
            break;
        }
        curr = curr->next;
    }
    return result;
}
