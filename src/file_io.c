#include "file_io.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "highlight.h"
#include "input.h"
#include "output.h"
#include "row.h"
#include "status.h"

static int isFileOpened(FileInfo info) {
    for (int i = 0; i < gEditor.file_count; i++) {
        if (areFilesEqual(gEditor.files[i].file_info, info)) {
            return i;
        }
    }
    return -1;
}

static char* editroRowsToString(EditorFile* file, size_t* len) {
    size_t total_len = 0;
    int nl_len = (file->newline == NL_UNIX) ? 1 : 2;
    for (int i = 0; i < file->num_rows; i++) {
        total_len += file->row[i].size + nl_len;
    }

    // last line no newline
    *len = (total_len > 0) ? total_len - nl_len : 0;

    char* buf = malloc_s(total_len);
    char* p = buf;
    for (int i = 0; i < file->num_rows; i++) {
        memcpy(p, file->row[i].data, file->row[i].size);
        p += file->row[i].size;
        if (i != file->num_rows - 1) {
            if (file->newline == NL_DOS) {
                *p = '\r';
                p++;
            }
            *p = '\n';
            p++;
        }
    }

    return buf;
}

static void editorExplorerFreeNode(EditorExplorerNode* node) {
    if (!node)
        return;

    if (node->is_directory) {
        for (size_t i = 0; i < node->dir.count; i++) {
            editorExplorerFreeNode(node->dir.nodes[i]);
        }

        for (size_t i = 0; i < node->file.count; i++) {
            editorExplorerFreeNode(node->file.nodes[i]);
        }

        free(node->dir.nodes);
        free(node->file.nodes);
    }

    free(node->filename);
    free(node);
}

bool editorOpen(EditorFile* file, const char* path) {
    editorInitFile(file);


    FileType type = getFileType(path);
    if (type == FT_INVALID)
        return false;

    if (type == FT_DIR) {
        if (gEditor.explorer.node)
            editorExplorerFreeNode(gEditor.explorer.node);
        gEditor.explorer.node = editorExplorerCreate(path);
        gEditor.explorer.node->is_open = true;
        editorExplorerRefresh();

        gEditor.explorer.offset = 0;
        gEditor.explorer.selected_index = 0;
        return false;
    }

    if (type != FT_REG) {
        editorSetStatusMsg("Can't load! \"%s\" is not a regular file.",
                           path);
        return false;
    }

    FileInfo file_info = getFileInfo(path);
    if (file_info.error) {
        editorSetStatusMsg("Can't load! Failed to get file info of \"%s\".",
                           path);
        return false;
    }
    file->file_info = file_info;
    int open_index = isFileOpened(file_info);

    if (open_index != -1) {
        editorChangeToFile(open_index);
        return false;
    }

    FILE* f = fopen(path, "rb");
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
        char parent_dir[EDITOR_PATH_MAX];
        snprintf(parent_dir, sizeof(parent_dir), "%s", path);
        getDirName(parent_dir);
        if (access(parent_dir, 0) != 0) {  // F_OK
            editorSetStatusMsg("Can't create \"%s\"! %s", path,
                               strerror(errno));
            return false;
        }
        if (access(parent_dir, 2) != 0) {  // W_OK
            editorSetStatusMsg("Can't write to \"%s\"! %s", path,
                               strerror(errno));
            return false;
        }
        editorInsertRow(file, file->cursor.y, "", 0);
    } else {
        bool has_end_nl = true;
        bool has_cr = false;
        size_t at = 0;
        size_t cap = 16;

        char* line = NULL;
        size_t n = 0;
        int64_t len;

        file->row = malloc_s(sizeof(EditorRow) * cap);

        while ((len = getLine(&line, &n, f)) != -1) {
            has_end_nl = false;
            while (len > 0 &&
                   (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                if (line[len - 1] == '\r')
                    has_cr = true;
                has_end_nl = true;
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
        file->lineno_width = getDigit(file->num_rows) + 2;

        if (has_end_nl) {
            editorInsertRow(file, file->num_rows, "", 0);
        }

        if (has_cr) {
            file->newline = NL_DOS;
        } else if (file->num_rows) {
            file->newline = NL_UNIX;
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
    size_t len;
    char* buf = editroRowsToString(file, &len);

    FILE* fp = fopen(file->filename, "wb");
    if (fp) {
        if (fwrite(buf, sizeof(char), len, fp) == len) {
            fclose(fp);
            free(buf);
            file->dirty = 0;
            editorSetStatusMsg("%d bytes written to disk.", len);
            return;
        }
        fclose(fp);
    }
    free(buf);
    editorSetStatusMsg("Can't save \"%s\"! %s", file->filename,
                       strerror(errno));
}

void editorOpenFilePrompt(void) {
    if (gEditor.file_count >= EDITOR_FILE_MAX_SLOT) {
        editorSetStatusMsg("Reached max file slots! Cannot open more files.",
                           strerror(errno));
        return;
    }

    char* path = editorPrompt("Open file: %s", OPEN_FILE_MODE, NULL);
    if (!path)
        return;

    EditorFile file;
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
    if (stat(path, &file_info) == -1)
        return NULL;

    EditorExplorerNode* node = malloc_s(sizeof(EditorExplorerNode));

    int len = strlen(path);
    node->filename = malloc_s(len + 1);
    snprintf(node->filename, len + 1, "%s", path);

    node->is_directory = S_ISDIR(file_info.st_mode);
    node->is_open = false;
    node->loaded = false;
    node->depth = 0;
    node->dir.count = 0;
    node->dir.nodes = NULL;
    node->file.count = 0;
    node->file.nodes = NULL;

    return node;
}

void editorExplorerLoadNode(EditorExplorerNode* node) {
    if (!node->is_directory)
        return;

    DirIter iter = dirFindFirst(node->filename);
    if (iter.error)
        return;

    do {
        const char* filename = dirGetName(iter);
        if (strcmp(filename, ".") == 0 ||
            strcmp(filename, "..") == 0)
            continue;

        char entry_path[EDITOR_PATH_MAX];
        snprintf(entry_path, sizeof(entry_path), PATH_CAT("%s","%s"), node->filename,
                 filename);

        EditorExplorerNode* child = editorExplorerCreate(entry_path);
        if (!child)
            continue;

        child->depth = node->depth + 1;

        if (child->is_directory) {
            insertExplorerNode(child, &node->dir);
        } else {
            insertExplorerNode(child, &node->file);
        }
    } while (dirNext(&iter));
    dirClose(&iter);

    node->loaded = true;
}

static void flattenNode(EditorExplorerNode* node) {
    if (node != gEditor.explorer.node)
        vector_push(gEditor.explorer.flatten, node);

    if (node->is_directory && node->is_open) {
        if (!node->loaded)
            editorExplorerLoadNode(node);

        for (size_t i = 0; i < node->dir.count; i++) {
            flattenNode(node->dir.nodes[i]);
        }

        for (size_t i = 0; i < node->file.count; i++) {
            flattenNode(node->file.nodes[i]);
        }
    }
}

void editorExplorerRefresh(void) {
    gEditor.explorer.flatten.size = 0;
    gEditor.explorer.flatten.capacity = 0;
    free(gEditor.explorer.flatten.data);
    flattenNode(gEditor.explorer.node);
    vector_shrink(gEditor.explorer.flatten);
}

void editorExplorerFree(void) {
    editorExplorerFreeNode(gEditor.explorer.node);
    free(gEditor.explorer.flatten.data);
}
