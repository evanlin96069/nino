#include <stdlib.h>
#include "editor.h"
#include "input.h"
#include "defines.h"

void editorGotoLine() {
    char* query = editorPrompt("Goto line: %s", GOTO_LINE_MODE, NULL);
    if (query == NULL)
        return;
    int line = atoi(query);
    if (line < 0) {
        line = E.num_rows + 1 + line;
    }
    if (line > 0 && line <= E.num_rows) {
        E.cx = 0;
        E.sx = 0;
        E.cy = line - 1;
    }
    if (query) {
        free(query);
    }
}
