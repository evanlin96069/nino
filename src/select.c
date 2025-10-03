#include "select.h"

#include "config.h"
#include "editor.h"
#include "os.h"
#include "row.h"
#include "utils.h"


void getSelectStartEnd(EditorSelectRange* range) {
    if (gCurFile->cursor.select_y > gCurFile->cursor.y) {
        range->start_x = gCurFile->cursor.x;
        range->start_y = gCurFile->cursor.y;
        range->end_x = gCurFile->cursor.select_x;
        range->end_y = gCurFile->cursor.select_y;
    } else if (gCurFile->cursor.select_y < gCurFile->cursor.y) {
        range->start_x = gCurFile->cursor.select_x;
        range->start_y = gCurFile->cursor.select_y;
        range->end_x = gCurFile->cursor.x;
        range->end_y = gCurFile->cursor.y;
    } else {
        // same row
        range->start_y = range->end_y = gCurFile->cursor.y;
        range->start_x = gCurFile->cursor.select_x > gCurFile->cursor.x
                             ? gCurFile->cursor.x
                             : gCurFile->cursor.select_x;
        range->end_x = gCurFile->cursor.select_x > gCurFile->cursor.x
                           ? gCurFile->cursor.select_x
                           : gCurFile->cursor.x;
    }
}

bool isPosSelected(int row, int col, EditorSelectRange range) {
    if (range.start_y < row && row < range.end_y)
        return true;

    if (range.start_y == row && range.end_y == row)
        return range.start_x <= col && col < range.end_x;

    if (range.start_y == row)
        return range.start_x <= col;

    if (range.end_y == row)
        return col < range.end_x;

    return false;
}

void editorDeleteText(EditorSelectRange range) {
    if (range.start_x == range.end_x && range.start_y == range.end_y)
        return;

    gCurFile->cursor.x = range.end_x;
    gCurFile->cursor.y = range.end_y;

    if (range.end_y - range.start_y > 1) {
        for (int i = range.start_y + 1; i < range.end_y; i++) {
            editorFreeRow(&gCurFile->row[i]);
        }
        int removed_rows = range.end_y - range.start_y - 1;
        memmove(&gCurFile->row[range.start_y + 1], &gCurFile->row[range.end_y],
                sizeof(EditorRow) * (gCurFile->num_rows - range.end_y));

        gCurFile->num_rows -= removed_rows;
        gCurFile->cursor.y -= removed_rows;

        gCurFile->lineno_width = getDigit(gCurFile->num_rows) + 2;
    }
    while (gCurFile->cursor.y != range.start_y ||
           gCurFile->cursor.x != range.start_x) {
        editorDelChar();
    }
}

void editorCopyText(EditorClipboard* clipboard, EditorSelectRange range) {
    if (range.start_x == range.end_x && range.start_y == range.end_y) {
        clipboard->size = 0;
        clipboard->lines = NULL;
        return;
    }

    clipboard->size = range.end_y - range.start_y + 1;
    clipboard->lines = malloc_s(sizeof(Str) * clipboard->size);

    size_t size;

    // Only one line
    if (range.start_y == range.end_y) {
        size = range.end_x - range.start_x;
        clipboard->lines[0].size = size;
        clipboard->lines[0].data = malloc_s(size);
        memcpy(clipboard->lines[0].data,
               &gCurFile->row[range.start_y].data[range.start_x],
               range.end_x - range.start_x);
        return;
    }

    // First line
    size = gCurFile->row[range.start_y].size - range.start_x;
    clipboard->lines[0].size = size;
    clipboard->lines[0].data = malloc_s(size);
    memcpy(clipboard->lines[0].data,
           &gCurFile->row[range.start_y].data[range.start_x], size);

    // Middle
    for (int i = range.start_y + 1; i < range.end_y; i++) {
        size = gCurFile->row[i].size;
        clipboard->lines[i - range.start_y].size = size;
        clipboard->lines[i - range.start_y].data = malloc_s(size);
        memcpy(clipboard->lines[i - range.start_y].data, gCurFile->row[i].data,
               size);
    }
    // Last line
    size = range.end_x;
    clipboard->lines[range.end_y - range.start_y].size = size;
    clipboard->lines[range.end_y - range.start_y].data = malloc_s(size);
    memcpy(clipboard->lines[range.end_y - range.start_y].data,
           gCurFile->row[range.end_y].data, size);
}

void editorPasteText(const EditorClipboard* clipboard, int x, int y) {
    if (!clipboard->size)
        return;

    gCurFile->cursor.x = x;
    gCurFile->cursor.y = y;

    if (clipboard->size == 1) {
        EditorRow* row = &gCurFile->row[y];
        char* paste = clipboard->lines[0].data;
        size_t paste_len = clipboard->lines[0].size;

        editorRowInsertString(gCurFile, row, x, paste, paste_len);
        gCurFile->cursor.x += paste_len;
    } else {
        // First line
        int auto_indent = CONVAR_GETINT(autoindent);
        CONVAR_GETINT(autoindent) = 0;
        editorInsertNewline();
        CONVAR_GETINT(autoindent) = auto_indent;
        editorRowAppendString(gCurFile, &gCurFile->row[y],
                              clipboard->lines[0].data,
                              clipboard->lines[0].size);
        // Middle
        for (size_t i = 1; i < clipboard->size - 1; i++) {
            editorInsertRow(gCurFile, y + i, clipboard->lines[i].data,
                            clipboard->lines[i].size);
        }
        // Last line
        EditorRow* row = &gCurFile->row[y + clipboard->size - 1];
        char* paste = clipboard->lines[clipboard->size - 1].data;
        size_t paste_len = clipboard->lines[clipboard->size - 1].size;
        editorRowInsertString(gCurFile, row, 0, paste, paste_len);

        gCurFile->cursor.y = y + clipboard->size - 1;
        gCurFile->cursor.x = paste_len;
    }
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}

void editorFreeClipboardContent(EditorClipboard* clipboard) {
    if (!clipboard || !clipboard->size)
        return;
    for (size_t i = 0; i < clipboard->size; i++) {
        free(clipboard->lines[i].data);
    }
    clipboard->size = 0;
    free(clipboard->lines);
}

void editorCopyToSysClipboard(EditorClipboard* clipboard, uint8_t newline) {
    if (!CONVAR_GETINT(osc52_copy))
        return;

    if (!clipboard || !clipboard->size)
        return;

    abuf ab = ABUF_INIT;
    for (size_t i = 0; i < clipboard->size; i++) {
        if (i != 0) {
            if (newline == NL_DOS)
                abufAppendN(&ab, "\r", 1);
            abufAppendN(&ab, "\n", 1);
        }
        abufAppendN(&ab, clipboard->lines[i].data, clipboard->lines[i].size);
    }

    int b64_len = base64EncodeLen(ab.len);
    char* b64_buf = malloc_s(b64_len * sizeof(char));

    b64_len = base64Encode(ab.buf, ab.len, b64_buf);

#ifndef _WIN32
    static bool tmux_check = false;
    static bool in_tmux;
    if (!tmux_check) {
        in_tmux = (getenv("TMUX") != NULL);
        tmux_check = true;
    }

    if (in_tmux) {
        fprintf(stdout, "\x1bPtmux;\x1b");
    }
#endif

    fprintf(stdout, "\x1b]52;c;%.*s\x07", b64_len, b64_buf);

#ifndef _WIN32
    if (in_tmux) {
        fprintf(stdout, "\x1b\\");
    }
#endif

    fflush(stdout);

    free(b64_buf);
    abufFree(&ab);
}
