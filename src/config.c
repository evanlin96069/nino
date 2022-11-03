#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "input.h"
#include "status.h"
#include "terminal.h"

static EditorConfig cfg = {
    .status_color = {{225, 219, 239}, {87, 80, 104}},
    .highlight_color = {{229, 229, 229},
                        {106, 153, 85},
                        {106, 153, 85},
                        {197, 134, 192},
                        {86, 156, 214},
                        {78, 201, 176},
                        {206, 145, 120},
                        {181, 206, 168},
                        {89, 46, 20},
                        {38, 79, 120}},
    .tab_size = 4,
    .whitespace = 0,
    .auto_indent = 0,
    .syntax = 0,
    .help_info = 1,
    .mouse = 0,
};

static int parseLine(char* line, int verbose) {
    char* token = strtok(line, " ");
    int argc = 0;
    char* argv[4];
    for (int i = 0; token && i < 4; i++, argc++) {
        argv[i] = token;
        token = strtok(NULL, " ");
    }

    if (argc < 1)
        return 0;

    if (strcmp(argv[0], "tabsize") == 0) {
        if (argc != 2) {
            if (verbose)
                editorSetStatusMsg("Usage: tabsize [size]");
            return 0;
        }
        int size = atoi(argv[1]);
        if (size < 1)
            return 0;
        E.cfg->tab_size = size;
    } else if (strcmp(argv[0], "whitespace") == 0) {
        if (argc != 2) {
            if (verbose)
                editorSetStatusMsg("Usage: whitespace [0|1]");
            return 0;
        }
        E.cfg->whitespace = !!atoi(argv[1]);
    } else if (strcmp(argv[0], "autoindent") == 0) {
        if (argc != 2) {
            if (verbose)
                editorSetStatusMsg("Usage: autoindent [0|1]");
            return 0;
        }
        E.cfg->auto_indent = !!atoi(argv[1]);
    } else if (strcmp(argv[0], "syntax") == 0) {
        if (argc != 2) {
            if (verbose)
                editorSetStatusMsg("Usage: syntax [0|1]");
            return 0;
        }
        E.cfg->syntax = !!atoi(argv[1]);
    } else if (strcmp(argv[0], "helpinfo") == 0) {
        if (argc != 2) {
            if (verbose)
                editorSetStatusMsg("Usage: helpinfo [0|1]");
            return 0;
        }
        E.cfg->help_info = !!atoi(argv[1]);
    } else if (strcmp(argv[0], "mouse") == 0) {
        if (argc != 2) {
            if (verbose)
                editorSetStatusMsg("Usage: mouse [0|1]");
            return 0;
        }
        int mouse = !!atoi(argv[1]);
        if (mouse)
            enableMouse();
        else
            disableMouse();
    } else if (strcmp(argv[0], "color") == 0) {
        if (argc != 3) {
            if (verbose)
                editorSetStatusMsg("Usage: color [target] [color]");
            return 0;
        }
        Color color = strToColor(argv[2]);
        if (strcmp(argv[1], "status.fg") == 0) {
            E.cfg->status_color[0] = color;
        } else if (strcmp(argv[1], "status.bg") == 0) {
            E.cfg->status_color[1] = color;
        } else if (strcmp(argv[1], "hl.normal") == 0) {
            E.cfg->highlight_color[HL_NORMAL] = color;
        } else if (strcmp(argv[1], "hl.comment") == 0) {
            E.cfg->highlight_color[HL_COMMENT] = color;
            E.cfg->highlight_color[HL_MLCOMMENT] = color;
        } else if (strcmp(argv[1], "hl.keyword1") == 0) {
            E.cfg->highlight_color[HL_KEYWORD1] = color;
        } else if (strcmp(argv[1], "hl.keyword2") == 0) {
            E.cfg->highlight_color[HL_KEYWORD2] = color;
        } else if (strcmp(argv[1], "hl.keyword3") == 0) {
            E.cfg->highlight_color[HL_KEYWORD3] = color;
        } else if (strcmp(argv[1], "hl.string") == 0) {
            E.cfg->highlight_color[HL_STRING] = color;
        } else if (strcmp(argv[1], "hl.number") == 0) {
            E.cfg->highlight_color[HL_NUMBER] = color;
        } else if (strcmp(argv[1], "hl.match") == 0) {
            E.cfg->highlight_color[HL_MATCH] = color;
        } else if (strcmp(argv[1], "hl.select") == 0) {
            E.cfg->highlight_color[HL_SELECT] = color;
        } else {
            if (verbose)
                editorSetStatusMsg("Unknown target %s.", argv[1]);
            return 0;
        }
    } else {
        if (verbose)
            editorSetStatusMsg("Unknown config %s.", argv[0]);
        return 0;
    }
    return 1;
}

void editorLoadConfig() {
    char path[255] = "";
    strncpy(path, getenv("HOME"), sizeof(path));
    strncat(path, "/.ninorc", sizeof(path) - 1);
    E.cfg = &cfg;
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
