#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "input.h"
#include "status.h"
#include "terminal.h"

CONVAR(tabsize, "Tab size.", "4");
CONVAR(whitespace, "Use whitespace instead of tab.", "0");
CONVAR(autoindent, "Enable auto indent.", "0");
CONVAR(backspace, "Use hungry backspace.", "0");
CONVAR(bracket, "Use auto bracket completion.", "0");
CONVAR(trailing, "Highlight trailing spaces.", "0");
CONVAR(syntax, "Enable syntax highlight.", "0");
CONVAR(helpinfo, "Show the help information.", "1");

CON_COMMAND(mouse, "Toggle. Enable mouse mode.") {
    UNUSED(args.argc);
    int mouse = !E.mouse_mode;
    if (mouse) {
        editorSetStatusMsg("Mouse mode ON.");
        enableMouse();
    } else {
        editorSetStatusMsg("Mouse mode OFF.");
        disableMouse();
    }
}

typedef struct {
    const char* label;
    Color* color;
} ColorElement;

static const ColorElement color_element_map[] = {
    {"bg", &E.color_cfg.bg},
    {"top.fg", &E.color_cfg.top_status[0]},
    {"top.bg", &E.color_cfg.top_status[1]},
    {"prompt.fg", &E.color_cfg.prompt[0]},
    {"prompt.bg", &E.color_cfg.prompt[1]},
    {"lineno.fg", &E.color_cfg.line_number[0]},
    {"lineno.bg", &E.color_cfg.line_number[1]},
    {"status.fg", &E.color_cfg.status[0]},
    {"status.bg", &E.color_cfg.status[1]},
    {"hl.normal", &E.color_cfg.highlight[HL_NORMAL]},
    {"hl.comment", &E.color_cfg.highlight[HL_COMMENT]},
    {"hl.keyword1", &E.color_cfg.highlight[HL_KEYWORD1]},
    {"hl.keyword2", &E.color_cfg.highlight[HL_KEYWORD2]},
    {"hl.keyword3", &E.color_cfg.highlight[HL_KEYWORD3]},
    {"hl.string", &E.color_cfg.highlight[HL_STRING]},
    {"hl.number", &E.color_cfg.highlight[HL_NUMBER]},
    {"hl.match", &E.color_cfg.highlight[HL_MATCH]},
    {"hl.select", &E.color_cfg.highlight[HL_SELECT]},
    {"hl.space", &E.color_cfg.highlight[HL_SPACE]},
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
        editorSetStatusMsg("Unknown element \"%s.\"", args.argv[1]);
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
    .bg = {0, 0, 0},
    .top_status = {{229, 229, 229}, {28, 28, 28}},
    .prompt = {{229, 229, 229}, {0, 0, 0}},
    .status = {{225, 219, 239}, {87, 80, 104}},
    .line_number = {{127, 127, 127}, {0, 0, 0}},
    .highlight = {{229, 229, 229},
                  {106, 153, 85},
                  {197, 134, 192},
                  {86, 156, 214},
                  {78, 201, 176},
                  {206, 145, 120},
                  {181, 206, 168},
                  {89, 46, 20},
                  {38, 79, 120},
                  {255, 100, 100}},
};

static void cvarCallback(EditorConCmd* thisptr, EditorConCmdArgs args) {
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
        cvarCallback(cmd, args);
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

    INIT_CONCOMMAND(mouse);
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
    for (int i = 0; i < E.num_rows; i++) {
        editorUpdateRow(&E.row[i]);
    }

    free(query);
}

void editorSetConVar(EditorConVar* thisptr, const char* string_val) {
    strncpy(thisptr->string_val, string_val, COMMAND_MAX_LENGTH);
    thisptr->string_val[COMMAND_MAX_LENGTH - 1] = '\0';
    thisptr->int_val = atoi(string_val);
}

static void registerConCmd(EditorConCmd* thisptr) {
    EditorConCmd* curr = E.cvars;
    if (!curr) {
        E.cvars = thisptr;
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
    EditorConCmd* curr = E.cvars;
    while (curr) {
        if (strcmp(name, curr->name) == 0) {
            result = curr;
            break;
        }
        curr = curr->next;
    }
    return result;
}
