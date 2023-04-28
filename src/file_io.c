#define _GNU_SOURCE

#include "file_io.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "editor.h"
#include "highlight.h"
#include "input.h"
#include "output.h"
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
            if (gEditor.explorer_node)
                editorExplorerFree(gEditor.explorer_node);
            gEditor.explorer_node = editorExplorerCreate(path);
            gEditor.explorer_node->is_open = true;
            gEditor.explorer_offset = 0;
            gEditor.explorer_last_line = 0;
            gEditor.explorer_select = 0;
            return false;
        }

        if (!S_ISREG(file_info.st_mode)) {
            editorSetStatusMsg("Can't load! \"%s\" is not a regular file.",
                               path);
            return false;
        }

        int open_index = isFileOpened(file_info.st_ino);
        if (open_index != -1) {
            editorChangeToFile(open_index);
            return false;
        }

        file->file_inode = file_info.st_ino;
    }

    FILE* f = fopen(path, "r");
    if (!f && errno != ENOENT) {
        editorSetStatusMsg("Can't load \"%s\"! %s", path, strerror(errno));
        return false;
    }

    free(file->filename);
    size_t fnlen = strlen(path) + 1;
    file->filename = malloc_s(fnlen);
    memcpy(file->filename, path, fnlen);
    editorSelectSyntaxHighlight(file);

    if (!f && errno == ENOENT) {
        // file doesn't exist
        char parent_dir[PATH_MAX];
        snprintf(parent_dir, sizeof(parent_dir), "%s", path);
        getDirName(parent_dir);
        if (access(parent_dir, F_OK) != 0) {
            editorSetStatusMsg("Can't create \"%s\"! %s", path,
                               strerror(errno));
            return false;
        }
        if (access(parent_dir, W_OK) != 0) {
            editorSetStatusMsg("Can't write to \"%s\"! %s", path,
                               strerror(errno));
            return false;
        }
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
    editorSetStatusMsg("Can't save \"%s\"! %s", file->filename,
                       strerror(errno));
}

void editorOpenFilePrompt() {
    if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT) {
        editorSetStatusMsg("Reached max file slots! Cannot open more files.",
                           strerror(errno));
        return;
    }

    char* path = editorPrompt("Open file: %s", OPEN_FILE_MODE, NULL);
    if (!path)
        return;

    EditorFile file = {0};
    if (editorOpen(&file, path)) {
        int index = editorAddFile(&file);
        // hack: refresh screen to update gEditor.tab_displayed
        editorRefreshScreen();
        editorChangeToFile(index);
    }

    free(path);
}

static void insertExplorerNode(EditorExplorerNode* node,
                               EditorExplorerNodeData* data) {
    size_t i;
    data->nodes =
        realloc_s(data->nodes, (data->count + 1) * sizeof(EditorExplorerNode*));

    for (i = 0; i < data->count; i++) {
        if (strcmp(data->nodes[i]->filename, node->filename) > 0) {
            memcpy(&data->nodes[i + 1], &data->nodes[i],
                   (data->count - i) * sizeof(EditorExplorerNode*));
            break;
        }
    }

    data->nodes[i] = node;
    data->count++;
}

EditorExplorerNode* editorExplorerCreate(const char* path) {
    struct stat file_info;
    if (lstat(path, &file_info) == -1)
        return NULL;

    EditorExplorerNode* node = malloc_s(sizeof(EditorExplorerNode));

    int len = strlen(path);
    node->filename = malloc_s(len + 1);
    snprintf(node->filename, len + 1, "%s", path);

    node->is_directory = S_ISDIR(file_info.st_mode);
    node->is_open = false;
    node->loaded = false;
    node->dir.count = 0;
    node->dir.nodes = NULL;
    node->file.count = 0;
    node->file.nodes = NULL;

    return node;
}

void editorExplorerLoadNode(EditorExplorerNode* node) {
    if (!node->is_directory)
        return;

    DIR* dir;
    if ((dir = opendir(node->filename)) == NULL)
        return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char entry_path[PATH_MAX];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", node->filename,
                 entry->d_name);

        EditorExplorerNode* child = editorExplorerCreate(entry_path);
        if (!child)
            continue;

        if (child->is_directory) {
            insertExplorerNode(child, &node->dir);
        } else {
            insertExplorerNode(child, &node->file);
        }
    }

    node->loaded = true;
    closedir(dir);
}

static EditorExplorerNode* walkNode(EditorExplorerNode* node, int* line,
                                    int index) {
    if (!node)
        return NULL;

    if (*line == index)
        return node;
    (*line)++;

    EditorExplorerNode* result = NULL;
    if (node->is_directory && node->is_open) {
        for (size_t i = 0; i < node->dir.count; i++) {
            if ((result = walkNode(node->dir.nodes[i], line, index)))
                return result;
        }

        for (size_t i = 0; i < node->file.count; i++) {
            if ((result = walkNode(node->file.nodes[i], line, index)))
                return result;
        }
    }
    return NULL;
}

EditorExplorerNode* editorExplorerSearch(int index) {
    if (index < 0)
        return NULL;
    int line = 0;
    return walkNode(gEditor.explorer_node, &line, index + 1);
}

void editorExplorerFree(EditorExplorerNode* node) {
    if (!node)
        return;

    if (node->is_directory) {
        for (size_t i = 0; i < node->dir.count; i++) {
            editorExplorerFree(node->dir.nodes[i]);
        }

        for (size_t i = 0; i < node->file.count; i++) {
            editorExplorerFree(node->file.nodes[i]);
        }

        free(node->dir.nodes);
        free(node->file.nodes);
    }

    free(node->filename);
    free(node);
}
