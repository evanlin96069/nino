#define _GNU_SOURCE

#include "file_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "defines.h"
#include "editor.h"
#include "highlight.h"
#include "input.h"
#include "row.h"
#include "status.h"
#include "utils.h"

static int isFileOpened(ino_t inode) {
    for (int i = 0; i < gEditor.file_count; i++) {
        if (gEditor.files[i].file_inode == inode) {
            return i;
        }
    }
    return -1;
}

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

bool editorOpen(EditorFile* file, const char* path) {
    struct stat file_info;
    if (lstat(path, &file_info) != -1) {
        if (S_ISDIR(file_info.st_mode)) {
            // TODO: Add file explorer
            editorSetStatusMsg("Can't load! \"%s\" is a directory.", path);
            return false;
        }

        if (!S_ISREG(file_info.st_mode)) {
            editorSetStatusMsg("Can't load! \"%s\" is not a regular file.", path);
            return false;
        }

        if (isFileOpened(file_info.st_ino) != -1) {
            return false;
        }

        file->file_inode = file_info.st_ino;
    }

    FILE* f = fopen(path, "r");
    if (!f && errno != ENOENT) {
        editorSetStatusMsg("Can't load! I/O error: %s", strerror(errno));
        return false;
    }


    free(file->filename);
    size_t fnlen = strlen(path) + 1;
    file->filename = malloc_s(fnlen);
    memcpy(file->filename, path, fnlen);
    editorSelectSyntaxHighlight(file);

    if (!f && errno == ENOENT) {
        editorInsertRow(file, file->cursor.y, "", 0);
    } else {
        bool end_nl = true;
        size_t at = 0;
        size_t cap = 16;

        char* line = NULL;
        size_t n = 0;
        ssize_t len;

        file->row = malloc_s(sizeof(EditorRow) * cap);

        while ((len = getline(&line, &n, f)) != -1) {
            end_nl = false;
            while (len > 0 &&
                   (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                end_nl = true;
                len--;
            }
            // editorInsertRow but faster
            if (at >= cap) {
                cap *= 2;
                file->row = realloc_s(file->row, sizeof(EditorRow) * cap);
            }
            file->row[at].size = len;
            file->row[at].data = line;

            file->row[at].hl = NULL;
            file->row[at].hl_open_comment = 0;
            editorUpdateRow(file, &file->row[at]);

            line = NULL;
            n = 0;
            at++;
        }
        file->row = realloc_s(file->row, sizeof(EditorRow) * at);
        file->num_rows = at;
        file->num_rows_digits = getDigit(file->num_rows);
        if (end_nl) {
            editorInsertRow(file, file->num_rows, "", 0);
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
