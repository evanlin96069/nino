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

static char* editroRowsToString(int* len) {
    int total_len = 0;
    for (int i = 0; i < gCurFile->num_rows; i++) {
        total_len += gCurFile->row[i].size + 1;
    }
    // last line no newline
    *len = total_len - 1;

    char* buf = malloc_s(total_len);
    char* p = buf;
    for (int i = 0; i < gCurFile->num_rows; i++) {
        memcpy(p, gCurFile->row[i].data, gCurFile->row[i].size);
        p += gCurFile->row[i].size;
        if (i != gCurFile->num_rows - 1)
            *p = '\n';
        p++;
    }

    return buf;
}

bool editorOpen(char* filename) {
    FILE* f = fopen(filename, "r+");

    if (!f && errno != ENOENT) {
        editorSetStatusMsg("Can't load! I/O error: %s", strerror(errno));
        return false;
    }

    free(gCurFile->filename);
    size_t fnlen = strlen(filename) + 1;
    gCurFile->filename = malloc_s(fnlen);
    memcpy(gCurFile->filename, filename, fnlen);
    editorSelectSyntaxHighlight();

    if (errno == ENOENT) {
        editorInsertRow(gCurFile->cursor.y, "", 0);
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
            editorInsertRow(gCurFile->num_rows, line, len);
        }
        if (end_nl) {
            editorInsertRow(gCurFile->num_rows, "", 0);
        }
        free(line);
        fclose(f);
    }
    gCurFile->dirty = 0;
    return true;
}

void editorSave(int save_as) {
    if (!gCurFile->filename || save_as) {
        char* filename = editorPrompt("Save as: %s", SAVE_AS_MODE, NULL);
        if (!filename) {
            editorSetStatusMsg("Save aborted.");
            return;
        }
        free(gCurFile->filename);
        gCurFile->filename = filename;
        editorSelectSyntaxHighlight();
    }
    int len;
    char* buf = editroRowsToString(&len);

    int fd = open(gCurFile->filename, O_RDWR | O_CREAT, 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                gCurFile->dirty = 0;
                editorSetStatusMsg("%d bytes written to disk.", len);
                return;
            }
        }
        close(fd);
    }
    free(buf);
    editorSetStatusMsg("Can't save! I/O error: %s", strerror(errno));
}
