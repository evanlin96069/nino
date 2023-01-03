#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "input.h"
#include "status.h"
#include "terminal.h"

CONVAR(tabsize, "Tab size.", "4");
CONVAR(whitespace, "Use whitespace instead of tab.", "0");
CONVAR(autoindent, "Enable auto indent.", "0");
CONVAR(syntax, "Enable syntax highlight.", "0");
CONVAR(helpinfo, "Show the help information.", "1");

CON_COMMAND(mouse, "Enable mouse mode.") {
    int mouse = !E.mouse_mode;
    if (mouse) {
        editorSetStatusMsg("Mouse mode ON.");
        enableMouse();
    } else {
        editorSetStatusMsg("Mouse mode OFF.");
        disableMouse();
    }
    return 1;
}

CON_COMMAND(color, "Usage: color [target] [color]") {
    if (args.argc != 3) {
        editorSetStatusMsg("Usage: color [target] [color]");
        return 0;
    }
    Color color = strToColor(args.argv[2]);
    if (strcmp(args.argv[1], "status.fg") == 0) {
        E.color_cfg->status[0] = color;
    } else if (strcmp(args.argv[1], "status.bg") == 0) {
        E.color_cfg->status[1] = color;
    } else if (strcmp(args.argv[1], "hl.normal") == 0) {
        E.color_cfg->highlight[HL_NORMAL] = color;
    } else if (strcmp(args.argv[1], "hl.comment") == 0) {
        E.color_cfg->highlight[HL_COMMENT] = color;
        E.color_cfg->highlight[HL_MLCOMMENT] = color;
    } else if (strcmp(args.argv[1], "hl.keyword1") == 0) {
        E.color_cfg->highlight[HL_KEYWORD1] = color;
    } else if (strcmp(args.argv[1], "hl.keyword2") == 0) {
        E.color_cfg->highlight[HL_KEYWORD2] = color;
    } else if (strcmp(args.argv[1], "hl.keyword3") == 0) {
        E.color_cfg->highlight[HL_KEYWORD3] = color;
    } else if (strcmp(args.argv[1], "hl.string") == 0) {
        E.color_cfg->highlight[HL_STRING] = color;
    } else if (strcmp(args.argv[1], "hl.number") == 0) {
        E.color_cfg->highlight[HL_NUMBER] = color;
    } else if (strcmp(args.argv[1], "hl.match") == 0) {
        E.color_cfg->highlight[HL_MATCH] = color;
    } else if (strcmp(args.argv[1], "hl.select") == 0) {
        E.color_cfg->highlight[HL_SELECT] = color;
    } else {
        editorSetStatusMsg("Unknown target %s.", args.argv[1]);
        return 0;
    }
    return 1;
}

CON_COMMAND(help, "Find help about a convar/concommand.") {
    if (args.argc != 2) {
        editorSetStatusMsg("Usage: help <command>");
        return 0;
    }
    EditorConCmd* cmd = editorFindCmd(args.argv[1]);
    if (!cmd) {
        editorSetStatusMsg("help: No cvar or command named \"%s\".",
                           args.argv[1]);
        return 0;
    }
    editorSetStatusMsg("\"%s\" - %s", cmd->name, cmd->help_string);
    return 0;
}

static EditorColorConfig cfg = {
    .status = {{225, 219, 239}, {87, 80, 104}},
    .highlight = {{229, 229, 229},
                  {106, 153, 85},
                  {106, 153, 85},
                  {197, 134, 192},
                  {86, 156, 214},
                  {78, 201, 176},
                  {206, 145, 120},
                  {181, 206, 168},
                  {89, 46, 20},
                  {38, 79, 120}},
};

static int cvarCallback(EditorConCmd* thisptr, EditorConCmdArgs args) {
    if (args.argc < 2) {
        editorSetStatusMsg("%s = %s - %s", thisptr->name,
                           thisptr->cvar.string_val, thisptr->help_string);
        return 0;
    }
    editorSetConVar(&thisptr->cvar, args.argv[1]);
    return 1;
}

static int parseLine(char* line, int verbose) {
    char* token = strtok(line, " ");
    EditorConCmdArgs args = {.argc = 0};
    for (int i = 0; token && i < 4; i++, args.argc++) {
        strcpy(args.argv[i], token);
        token = strtok(NULL, " ");
    }

    if (args.argc < 1)
        return 0;

    EditorConCmd* cmd = editorFindCmd(args.argv[0]);
    if (!cmd) {
        if (verbose)
            editorSetStatusMsg("Unknown command \"%s\".", args.argv[0]);
        return 0;
    }

    int result = 0;
    if (cmd->has_callback)
        result = cmd->callback(cmd, args);
    else
        result = cvarCallback(cmd, args);

    if (!verbose)
        editorSetStatusMsg("");
    return result;
}

void editorInitCommands() {
    INIT_CONVAR(tabsize);
    INIT_CONVAR(whitespace);
    INIT_CONVAR(autoindent);
    INIT_CONVAR(syntax);
    INIT_CONVAR(helpinfo);

    INIT_CONCOMMAND(mouse);
    INIT_CONCOMMAND(color);
    INIT_CONCOMMAND(help);
}

void editorLoadConfig() {
    char path[255] = "";
    strncpy(path, getenv("HOME"), sizeof(path));
    strncat(path, "/.ninorc", sizeof(path) - 1);
    E.color_cfg = &cfg;
    FILE* fp = fopen(path, "r");
    if (!fp)
        return;

    char buf[128];
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strcspn(buf, "\r\n")] = '\0';
        parseLine(buf, 0);
    }

    fclose(fp);
}

void editorSetting() {
    char* query = editorPrompt("Config: %s", SETTING_MODE, NULL);
    if (query == NULL)
        return;
    if (parseLine(query, 1)) {
        for (int i = 0; i < E.num_rows; i++) {
            editorUpdateRow(&E.row[i]);
        }
    }
    if (query) {
        free(query);
    }
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
    thisptr->has_callback = 1;
    registerConCmd(thisptr);
}

void editorInitConVar(EditorConCmd* thisptr) {
    thisptr->next = NULL;
    thisptr->has_callback = 0;
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
