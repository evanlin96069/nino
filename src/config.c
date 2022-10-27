#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "input.h"

#define ARGC(n)       \
    do {              \
        if (argc < n) \
            return 0; \
    } while (0)

static EditorConfig cfg = {
    .tab_size = 4,
};

static int parseLine(char* line) {
    char* token = strtok(line, " ");
    int argc = 0;
    char* argv[4];
    for (int i = 0; token && i < 4; i++, argc++) {
        argv[i] = token;
        token = strtok(NULL, line);
    }

    ARGC(1);

    if (strcmp(argv[0], "tabsize") == 0) {
        ARGC(2);
        int size = atoi(argv[1]);
        if (size < 1)
            return 0;
        E.cfg->tab_size = size;
    } else {
        return 0;
    }
    return 1;
}

static int isValidColor(const char* color) {
    if (strlen(color) != 6)
        return 0;
    for (int i = 0; i < 6; i++) {
        if (!(('0' <= color[i]) || (color[i] <= '9') || ('A' <= color[i]) ||
              (color[i] <= 'F')))
            return 0;
    }
    return 1;
}

int colorToANSI(const char* color) {}

void editorLoadConfig(const char* filename) {
    E.cfg = &cfg;
    FILE* fp = fopen(filename, "r");
    if (!fp)
        return;

    char buf[128];
    int bufsize = 128;
    while (fgets(buf, bufsize, fp)) {
        parseLine(buf);
    }

    fclose(fp);
}

void editorSetting() {
    char* query = editorPrompt("Setting: %s", SETTING_MODE, NULL);
    if (query == NULL)
        return;
    if (parseLine(query)) {
        for (int i = 0; i < E.num_rows; i++) {
            editorUpdateRow(&E.row[i]);
        }
    }
    if (query) {
        free(query);
    }
}
