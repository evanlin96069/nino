#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "input.h"
#include "utils.h"

static void editorFindCallback(char* query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static unsigned char* saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    int len = strlen(query);
    if (len == 0)
        return;

    if (key == ESC || (last_match != -1 && key == '\r')) {
        last_match = -1;
        direction = 1;
        return;
    }
    if (key == ARROW_RIGHT || key == ARROW_DOWN || key == '\r') {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
    }

    if (last_match == -1)
        direction = 1;
    int current = last_match;
    for (int i = 0; i < E.num_rows; i++) {
        current += direction;
        if (current == -1)
            current = E.num_rows - 1;
        else if (current == E.num_rows)
            current = 0;
        EditorRow* row = &(E.row[current]);
        char* match = strstr(row->render, query);
        if (match) {
            match += len;
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.sx = match - row->render;
            E.row_offset = E.num_rows;
            if (key == '\r') {
                last_match = -1;
                direction = 1;
                return;
            }
            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&(row->hl[match - len - row->render]), HL_MATCH, len);
            break;
        }
    }
}

void editorFind() {
    char* query = editorPrompt("Search: %s", FIND_MODE, editorFindCallback);
    if (query) {
        free(query);
    }
}
