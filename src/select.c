#include "select.h"

#include "config.h"
#include "editor.h"
#include "os.h"
#include "row.h"
#include "unicode.h"
#include "utils.h"

void getSelectStartEnd(const EditorCursor* cursor, EditorSelectRange* range) {
    if (!cursor->is_selected) {
        range->start_x = range->end_x = cursor->x;
        range->start_y = range->end_y = cursor->y;
        return;
    }

    if (cursor->select_y > cursor->y) {
        range->start_x = cursor->x;
        range->start_y = cursor->y;
        range->end_x = cursor->select_x;
        range->end_y = cursor->select_y;
    } else if (cursor->select_y < cursor->y) {
        range->start_x = cursor->select_x;
        range->start_y = cursor->select_y;
        range->end_x = cursor->x;
        range->end_y = cursor->y;
    } else {
        // same row
        range->start_y = range->end_y = cursor->y;
        range->start_x =
            cursor->select_x > cursor->x ? cursor->x : cursor->select_x;
        range->end_x =
            cursor->select_x > cursor->x ? cursor->select_x : cursor->x;
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

EditorSelectRange getClipboardRange(int x,
                                    int y,
                                    const EditorClipboard* clipboard) {
    EditorSelectRange range = {
        .start_x = x,
        .start_y = y,
    };
    if (!clipboard || clipboard->size == 0) {
        range.end_x = x;
        range.end_y = y;
        return range;
    }

    range.end_y = y + (int)clipboard->size - 1;
    if (clipboard->size == 1) {
        range.end_x = x + clipboard->lines[0].size;
    } else {
        range.end_x = clipboard->lines[clipboard->size - 1].size;
    }
    return range;
}

void editorDeleteText(EditorFile* file, EditorSelectRange range) {
    if (range.start_x == range.end_x && range.start_y == range.end_y)
        return;

    if (range.start_y == range.end_y) {
        EditorRow* row = &file->row[range.start_y];
        if (range.start_x < range.end_x) {
            memmove(&row->data[range.start_x], &row->data[range.end_x],
                    row->size - range.end_x);
            row->size -= range.end_x - range.start_x;
            editorUpdateRow(file, row);
        }
        return;
    }

    EditorRow* start_row = &file->row[range.start_y];
    EditorRow* end_row = &file->row[range.end_y];
    int tail_len = end_row->size - range.end_x;

    start_row->size = range.start_x;
    if (tail_len > 0) {
        editorRowAppendString(file, start_row, &end_row->data[range.end_x],
                              tail_len);
    } else {
        editorUpdateRow(file, start_row);
    }

    for (int i = range.start_y + 1; i <= range.end_y; i++) {
        editorFreeRow(&file->row[i]);
    }

    int removed_rows = range.end_y - range.start_y;
    memmove(&file->row[range.start_y + 1], &file->row[range.end_y + 1],
            sizeof(EditorRow) * (file->num_rows - range.end_y - 1));

    file->num_rows -= removed_rows;
    file->lineno_width = getDigit(file->num_rows) + 2;

    if (range.start_y + 1 < file->num_rows) {
        editorUpdateRow(file, &file->row[range.start_y + 1]);
    }
}

void editorCopyText(EditorFile* file,
                    EditorClipboard* clipboard,
                    EditorSelectRange range) {
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
               &file->row[range.start_y].data[range.start_x],
               range.end_x - range.start_x);
        return;
    }

    // First line
    size = file->row[range.start_y].size - range.start_x;
    clipboard->lines[0].size = size;
    clipboard->lines[0].data = malloc_s(size);
    memcpy(clipboard->lines[0].data,
           &file->row[range.start_y].data[range.start_x], size);

    // Middle
    for (int i = range.start_y + 1; i < range.end_y; i++) {
        size = file->row[i].size;
        clipboard->lines[i - range.start_y].size = size;
        clipboard->lines[i - range.start_y].data = malloc_s(size);
        memcpy(clipboard->lines[i - range.start_y].data, file->row[i].data,
               size);
    }
    // Last line
    size = range.end_x;
    clipboard->lines[range.end_y - range.start_y].size = size;
    clipboard->lines[range.end_y - range.start_y].data = malloc_s(size);
    memcpy(clipboard->lines[range.end_y - range.start_y].data,
           file->row[range.end_y].data, size);
}

void editorCopyLine(EditorFile* file, EditorClipboard* clipboard, int row) {
    if (row < 0 || row >= file->num_rows) {
        clipboard->size = 0;
        clipboard->lines = NULL;
        return;
    }

    clipboard->size = 2;
    clipboard->lines = malloc_s(sizeof(Str) * clipboard->size);

    // First line
    size_t size = file->row[row].size;
    clipboard->lines[0].size = size;
    clipboard->lines[0].data = malloc_s(size);
    memcpy(clipboard->lines[0].data, &file->row[row].data[0], size);
    // Empty line
    clipboard->lines[1].size = 0;
    clipboard->lines[1].data = NULL;
}

void editorPasteText(EditorFile* file,
                     const EditorClipboard* clipboard,
                     int x,
                     int y) {
    if (!clipboard->size)
        return;

    if (clipboard->size == 1) {
        EditorRow* row = &file->row[y];
        char* paste = clipboard->lines[0].data;
        size_t paste_len = clipboard->lines[0].size;

        editorRowInsertString(file, row, x, paste, paste_len);
    } else {
        // First line
        size_t tail_len = file->row[y].size - x;
        editorInsertRow(file, y + 1, &file->row[y].data[x], tail_len);
        file->row[y].size = x;
        editorRowAppendString(file, &file->row[y], clipboard->lines[0].data,
                              clipboard->lines[0].size);
        // Middle
        for (size_t i = 1; i < clipboard->size - 1; i++) {
            editorInsertRow(file, y + i, clipboard->lines[i].data,
                            clipboard->lines[i].size);
        }
        // Last line
        EditorRow* row = &file->row[y + clipboard->size - 1];
        char* paste = clipboard->lines[clipboard->size - 1].data;
        size_t paste_len = clipboard->lines[clipboard->size - 1].size;
        editorRowInsertString(file, row, 0, paste, paste_len);
    }
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

    abufReset(&ab);

#ifndef _WIN32
    static bool tmux_check = false;
    static bool in_tmux;
    if (!tmux_check) {
        in_tmux = (getenv("TMUX") != NULL);
        tmux_check = true;
    }

    if (in_tmux) {
        abufAppendStr(&ab, "\x1bPtmux;\x1b");
    }
#endif
    abufAppendStr(&ab, "\x1b]52;c;");
    abufAppendN(&ab, b64_buf, b64_len);
    abufAppendStr(&ab, "\x07");

#ifndef _WIN32
    if (in_tmux) {
        abufAppendStr(&ab, "\x1b\\");
    }
#endif

    writeConsoleAll(ab.buf, ab.len);

    free(b64_buf);
    abufFree(&ab);
}

static void editorClipboardEnsureLine(EditorClipboard* clipboard,
                                      size_t index) {
    if (!clipboard)
        return;

    if (clipboard->size > index)
        return;

    size_t new_size = index + 1;
    clipboard->lines = realloc_s(clipboard->lines, sizeof(Str) * new_size);
    for (size_t i = clipboard->size; i < new_size; i++) {
        clipboard->lines[i].data = NULL;
        clipboard->lines[i].size = 0;
    }
    clipboard->size = new_size;
}

void editorClipboardAppendAt(EditorClipboard* clipboard,
                             size_t line_index,
                             const char* data,
                             size_t len) {
    if (!clipboard || len == 0)
        return;

    editorClipboardEnsureLine(clipboard, line_index);

    Str* line = &clipboard->lines[line_index];
    int new_size = line->size + (int)len;
    line->data = realloc_s(line->data, (size_t)new_size);
    memcpy(&line->data[line->size], data, len);
    line->size = new_size;
}

void editorClipboardAppendAtRepeat(EditorClipboard* clipboard,
                                   size_t line_index,
                                   char value,
                                   size_t count) {
    if (!clipboard || count == 0)
        return;

    editorClipboardEnsureLine(clipboard, line_index);

    Str* line = &clipboard->lines[line_index];
    int new_size = line->size + (int)count;
    line->data = realloc_s(line->data, (size_t)new_size);
    memset(&line->data[line->size], value, count);
    line->size = new_size;
}

void editorClipboardAppendChar(EditorClipboard* clipboard, char c) {
    if (!clipboard)
        return;

    if (clipboard->size == 0) {
        editorClipboardEnsureLine(clipboard, 0);
    }

    editorClipboardAppendAt(clipboard, clipboard->size - 1, &c, 1);
}

void editorClipboardAppendUnicode(EditorClipboard* clipboard,
                                  uint32_t unicode) {
    char output[4];
    int len = encodeUTF8(unicode, output);
    if (len == -1)
        return;

    editorClipboardAppendAt(clipboard,
                            clipboard->size == 0 ? 0 : clipboard->size - 1,
                            output, (size_t)len);
}

void editorClipboardAppendNewline(EditorClipboard* clipboard) {
    if (!clipboard)
        return;

    editorClipboardEnsureLine(clipboard, clipboard->size);
}
