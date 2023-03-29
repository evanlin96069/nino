#include <stdlib.h>

#include "defines.h"
#include "editor.h"
#include "input.h"
#include "status.h"

static void editorGotoCallback(char* query, int key) {
    if (query == NULL || key == ESC)
        return;

    int line = atoi(query);
    if (line < 0) {
        line = gCurFile->num_rows + 1 + line;
    }

    if (line > 0 && line <= gCurFile->num_rows) {
        gCurFile->cursor.x = 0;
        gCurFile->sx = 0;
        gCurFile->cursor.y = line - 1;
        editorScrollToCursorCenter();
    } else {
        editorSetStatusMsg("Type a line number between 1 to %d.",
                           gCurFile->num_rows);
    }
}

void editorGotoLine() {
    char* query =
        editorPrompt("Goto line: %s", GOTO_LINE_MODE, editorGotoCallback);
    if (query) {
        free(query);
    }
}
