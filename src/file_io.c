#include "file_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "defines.h"
#include "editor.h"
#include "highlight.h"
#include "input.h"
#include "row.h"
#include "status.h"

static char* editroRowsToString(EditorFile* file, int* len) {
    int total_len = 0;
    for (int i = 0; i < file->num_rows; i++) {
        total_len += file->row[i].size + 1;
    }
    // last line no newline
    *len = total_len - 1;

    char* buf = malloc_s(total_len);
    char* p = buf;
    for (int i = 0; i < file->num_rows; i++) {
        memcpy(p, file->row[i].data, file->row[i].size);
        p += file->row[i].size;
        if (i != file->num_rows - 1)
            *p = '\n';
        p++;
    }

    return buf;
}

bool editorOpen(EditorFile* file, char* filename) {
    FILE* f = fopen(filename, "r");

    if (!f && errno != ENOENT) {
        editorSetStatusMsg("Can't load! I/O error: %s", strerror(errno));
        return false;
    }

    free(file->filename);
    size_t fnlen = strlen(filename) + 1;
    file->filename = malloc_s(fnlen);
    memcpy(file->filename, filename, fnlen);
    editorSelectSyntaxHighlight(file);

    if (errno == ENOENT) {
        editorInsertRow(file->cursor.y, "", 0);
    } else {
        char* line = NULL;
        size_t cap = 0;
        ssize_t len;
        int end_nl = 1;
        while ((len = getline(&line, &cap, f)) != -1) {
            end_nl = 0;
            while (len > 0 &&
                   (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                end_nl = 1;
                len--;
            }
            editorInsertRow(file->num_rows, line, len);
        }
        if (end_nl) {
            editorInsertRow(file->num_rows, "", 0);
        }
        free(line);
        fclose(f);
    }
    file->dirty = 0;
    return true;
}

void editorSave(EditorFile* file, int save_as) {
    if (!file->filename || save_as) {
        char* filename = editorPrompt("Save as: %s", SAVE_AS_MODE, NULL);
        if (!filename) {
            editorSetStatusMsg("Save aborted.");
            return;
        }
        free(file->filename);
        file->filename = filename;
        editorSelectSyntaxHighlight(file);
    }
    int len;
    char* buf = editroRowsToString(file, &len);

    int fd = open(file->filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                file->dirty = 0;
                editorSetStatusMsg("%d bytes written to disk.", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMsg("Can't save! I/O error: %s", strerror(errno));
}
