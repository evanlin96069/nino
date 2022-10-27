#include "editor.h"
#include "terminal.h"
#include "file_io.h"
#include "row.h"
#include "input.h"
#include "output.h"
#include "config.h"

int main(int argc, char* argv[]) {
    enableRawMode();
    editorLoadConfig("~/.ninorc");
    editorInit();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }
    else {
        editorInsertRow(E.cy, "", 0);
        E.dirty = 0;
    }
    E.cols -= E.num_rows_digits + 1;
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
        if (getWindowSize(&E.rows, &E.cols) == -1)
            DIE("getWindowSize");
        E.rows -= 3;
        E.cols -= E.num_rows_digits + 1;
    }
    return 0;
}
