#include "row.h"

#include "editor.h"
#include "highlight.h"
#include "unicode.h"
#include "utils.h"

static inline bool ensureCapacity(size_t capacity,
                                  size_t size,
                                  size_t* new_capacity) {
    if (capacity >= size)
        return false;

    *new_capacity = capacity ? capacity : 8;
    while (*new_capacity < size) {
        if (*new_capacity < 1024) {
            (*new_capacity) *= 2;
        } else {
            (*new_capacity) += (*new_capacity) / 2;
        }
    }
    return true;
}

static void editorRowEnsureCapacity(EditorRow* row, size_t size) {
    size_t new_capacity;
    if (!ensureCapacity(row->capacity, size, &new_capacity))
        return;

    row->data = realloc_s(row->data, new_capacity);
    row->hl = realloc_s(row->hl, new_capacity);
    row->capacity = new_capacity;
}

void editorUpdateRow(EditorFile* file, EditorRow* row) {
    row->rsize = editorRowCxToRx(row, row->size);
    editorUpdateSyntax(file, row);
}

void editorInsertRow(EditorFile* file, int at, const char* s, size_t len) {
    if (at < 0 || at > file->num_rows)
        return;

    size_t new_capacity;
    if (ensureCapacity(file->row_capacity, file->num_rows + 1, &new_capacity)) {
        file->row = realloc_s(file->row, sizeof(EditorRow) * new_capacity);
    }

    memmove(&file->row[at + 1], &file->row[at],
            sizeof(EditorRow) * (file->num_rows - at));
    memset(&file->row[at], 0, sizeof(EditorRow));
    editorRowAppendString(file, &file->row[at], s, len);

    file->num_rows++;
    file->lineno_width = getDigit(file->num_rows) + 2;
}

void editorFreeRow(EditorRow* row) {
    free(row->data);
    free(row->hl);
}

void editorDelRow(EditorFile* file, int at) {
    if (at < 0 || at >= file->num_rows)
        return;
    editorFreeRow(&file->row[at]);
    memmove(&file->row[at], &file->row[at + 1],
            sizeof(EditorRow) * (file->num_rows - at - 1));

    file->num_rows--;
    file->lineno_width = getDigit(file->num_rows) + 2;
}

void editorRowInsertChar(EditorFile* file, EditorRow* row, int at, int c) {
    if (at < 0 || at > row->size)
        return;
    editorRowEnsureCapacity(row, row->size + 1);
    memmove(&row->data[at + 1], &row->data[at], row->size - at);
    row->size++;
    row->data[at] = c;
    editorUpdateRow(file, row);
}

void editorRowDelChar(EditorFile* file, EditorRow* row, int at) {
    if (at < 0 || at >= row->size)
        return;
    memmove(&row->data[at], &row->data[at + 1], row->size - at - 1);
    row->size--;
    editorUpdateRow(file, row);
}

void editorRowAppendString(EditorFile* file,
                           EditorRow* row,
                           const char* s,
                           size_t len) {
    editorRowEnsureCapacity(row, row->size + len);
    memcpy(&row->data[row->size], s, len);
    row->size += len;
    editorUpdateRow(file, row);
}

void editorRowInsertString(EditorFile* file,
                           EditorRow* row,
                           int at,
                           const char* s,
                           size_t len) {
    if (at < 0 || at > row->size)
        return;

    editorRowEnsureCapacity(row, row->size + len);
    memmove(&row->data[at + len], &row->data[at], row->size - at);
    memcpy(&row->data[at], s, len);
    row->size += len;
    editorUpdateRow(file, row);
}

void editorInsertChar(int c) {
    if (gCurFile->cursor.y == gCurFile->num_rows) {
        editorInsertRow(gCurFile, gCurFile->num_rows, "", 0);
    }
    if (c == '\t' && CONVAR_GETINT(whitespace)) {
        int idx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                  gCurFile->cursor.x) +
                  1;
        editorInsertChar(' ');
        while (idx % CONVAR_GETINT(tabsize) != 0) {
            editorInsertChar(' ');
            idx++;
        }
    } else {
        editorRowInsertChar(gCurFile, &gCurFile->row[gCurFile->cursor.y],
                            gCurFile->cursor.x, c);
        gCurFile->cursor.x++;
    }
}

void editorInsertUnicode(uint32_t unicode) {
    char output[4];
    int len = encodeUTF8(unicode, output);
    if (len == -1)
        return;

    for (int i = 0; i < len; i++) {
        editorInsertChar(output[i]);
    }
}

void editorInsertNewline(void) {
    int i = 0;

    if (gCurFile->cursor.x == 0) {
        editorInsertRow(gCurFile, gCurFile->cursor.y, "", 0);
    } else {
        editorInsertRow(gCurFile, gCurFile->cursor.y + 1, "", 0);
        EditorRow* curr_row = &gCurFile->row[gCurFile->cursor.y];
        EditorRow* new_row = &gCurFile->row[gCurFile->cursor.y + 1];
        if (CONVAR_GETINT(autoindent)) {
            while (i < gCurFile->cursor.x &&
                   (curr_row->data[i] == ' ' || curr_row->data[i] == '\t')) {
                i++;
            }

            if (i != 0) {
                editorRowAppendString(gCurFile, new_row, curr_row->data, i);
            }

            // TODO: language specific auto indent
            bool should_inc = false;

            char prev = curr_row->data[gCurFile->cursor.x - 1];
            if (prev == ':') {
                // Python
                should_inc = true;
            } else if (prev == '{') {
                // C
                if (gCurFile->cursor.x < curr_row->size) {
                    should_inc = (curr_row->data[gCurFile->cursor.x] != '}');
                } else {
                    should_inc = true;
                }
            }

            if (should_inc) {
                if (CONVAR_GETINT(whitespace)) {
                    for (int j = 0; j < CONVAR_GETINT(tabsize); j++, i++)
                        editorRowAppendString(gCurFile, new_row, " ", 1);
                } else {
                    editorRowAppendString(gCurFile, new_row, "\t", 1);
                    i++;
                }
            }
        }
        editorRowAppendString(gCurFile, new_row,
                              &curr_row->data[gCurFile->cursor.x],
                              curr_row->size - gCurFile->cursor.x);
        curr_row->size = gCurFile->cursor.x;
        editorUpdateRow(gCurFile, curr_row);
    }
    gCurFile->cursor.y++;
    gCurFile->cursor.x = i;
    gCurFile->sx = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], i);
}

void editorDelChar(void) {
    if (gCurFile->cursor.y == gCurFile->num_rows)
        return;
    if (gCurFile->cursor.x == 0 && gCurFile->cursor.y == 0)
        return;
    EditorRow* row = &gCurFile->row[gCurFile->cursor.y];
    if (gCurFile->cursor.x > 0) {
        editorRowDelChar(gCurFile, row, gCurFile->cursor.x - 1);
        gCurFile->cursor.x--;
    } else {
        gCurFile->cursor.x = gCurFile->row[gCurFile->cursor.y - 1].size;
        editorRowAppendString(gCurFile, &gCurFile->row[gCurFile->cursor.y - 1],
                              row->data, row->size);
        editorDelRow(gCurFile, gCurFile->cursor.y);
        gCurFile->cursor.y--;
    }
    gCurFile->sx =
        editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y], gCurFile->cursor.x);
}

int editorRowNextUTF8(EditorRow* row, int cx) {
    if (cx < 0)
        return 0;

    if (cx >= row->size)
        return row->size;

    const char* s = &row->data[cx];
    size_t byte_size;
    decodeUTF8(s, row->size - cx, &byte_size);
    return cx + byte_size;
}

int editorRowPreviousUTF8(EditorRow* row, int cx) {
    if (cx <= 0)
        return 0;

    if (cx > row->size)
        return row->size;

    int i = 0;
    size_t byte_size = 0;
    while (i < cx) {
        decodeUTF8(&row->data[i], row->size - i, &byte_size);
        i += byte_size;
    }
    return i - byte_size;
}

int editorRowCxToRx(const EditorRow* row, int cx) {
    int rx = 0;
    int i = 0;
    while (i < cx) {
        size_t byte_size;
        uint32_t unicode = decodeUTF8(&row->data[i], row->size - i, &byte_size);
        if (unicode == '\t') {
            rx += (CONVAR_GETINT(tabsize) - 1) - (rx % CONVAR_GETINT(tabsize)) +
                  1;
        } else {
            int width = unicodeWidth(unicode);
            if (width < 0)
                width = 1;
            rx += width;
        }
        i += byte_size;
    }
    return rx;
}

int editorRowRxToCx(const EditorRow* row, int rx) {
    int cur_rx = 0;
    int cx = 0;
    while (cx < row->size) {
        size_t byte_size;
        uint32_t unicode =
            decodeUTF8(&row->data[cx], row->size - cx, &byte_size);
        if (unicode == '\t') {
            cur_rx += (CONVAR_GETINT(tabsize) - 1) -
                      (cur_rx % CONVAR_GETINT(tabsize)) + 1;
        } else {
            int width = unicodeWidth(unicode);
            if (width < 0)
                width = 1;
            cur_rx += width;
        }
        if (cur_rx > rx)
            return cx;
        cx += byte_size;
    }
    return cx;
}
