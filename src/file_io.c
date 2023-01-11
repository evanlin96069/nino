#include "file_io.h"

#define _GNU_SOURCE

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

static char* editroRowsToString(int* len) {
    int total_len = 0;
    for (int i = 0; i < E.num_rows; i++) {
        total_len += E.row[i].size + 1;
    }
    // last line no newline
    *len = total_len - 1;

    char* buf = malloc_s(total_len);
    char* p = buf;
    for (int i = 0; i < E.num_rows; i++) {
        memcpy(p, E.row[i].data, E.row[i].size);
        p += E.row[i].size;
        if (i != E.num_rows - 1)
            *p = '\n';
        p++;
    }

    return buf;
}

void editorOpen(char* filename) {
    free(E.filename);
    size_t fnlen = strlen(filename) + 1;
    E.filename = malloc_s(fnlen);
    memcpy(E.filename, filename, fnlen);

    editorSelectSyntaxHighlight();

    FILE* f = fopen(filename, "r");
    if (f) {
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
            editorInsertRow(E.num_rows, line, len);
        }
        if (end_nl) {
            editorInsertRow(E.num_rows, "", 0);
        }
        free(line);
        fclose(f);
    } else {
        editorInsertRow(E.cursor.y, "", 0);
    }
    E.dirty = 0;
}

void editorSave(int save_as) {
    if (!E.filename || save_as) {
        char* filename = editorPrompt("Save as: %s", SAVE_AS_MODE, NULL);
        if (!filename) {
            editorSetStatusMsg("Save aborted.");
            return;
        }
        free(E.filename);
        E.filename = filename;
        editorSelectSyntaxHighlight();
    }
    int len;
    char* buf = editroRowsToString(&len);

    int fd = open(E.filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMsg("%d bytes written to disk.", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMsg("Can't save! I/O error: %s", strerror(errno));
}
