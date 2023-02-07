#include "editor.h"
#include "file_io.h"
#include "input.h"
#include "output.h"
#include "row.h"

int main(int argc, char* argv[]) {
    editorInit();

    if (argc < 2 || !editorOpen(argv[1])) {
        editorInsertRow(0, "", 0);
    }

    E.loading = false;

    while (true) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
