#include "config.h"
#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "output.h"
#include "row.h"
#include "terminal.h"

int main(int argc, char* argv[]) {
    enableRawMode();
    editorLoadConfig("~/.ninorc");
    editorInit();
    if (argc >= 2) {
        editorOpen(argv[1]);
    } else {
        editorInsertRow(E.cy, "", 0);
        E.dirty = 0;
    }
    E.cols -= E.num_rows_digits + 1;
    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
