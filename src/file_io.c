#include "file_io.h"

#include <errno.h>
#include <fcntl.h>

#include "config.h"
#include "editor.h"
#include "highlight.h"
#include "input.h"
#include "prompt.h"
#include "row.h"

static int isFileOpened(FileInfo info) {
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count > 0 &&
            gEditor.files[i].has_file_info &&
            areFilesEqual(gEditor.files[i].file_info, info)) {
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
        if (file->row[i].size > 0) {
            memcpy(p, file->row[i].data, file->row[i].size);
            p += file->row[i].size;
        }
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

static void editorLoadRowsFromStream(EditorFile* file, FILE* fp) {
    bool has_end_nl = true;
    bool has_cr = false;
    size_t at = 0;

    char* line = NULL;
    size_t n = 0;
    int64_t len;

    file->row = malloc_s(sizeof(EditorRow) * 16);

    while ((len = getLine(&line, &n, fp)) != -1) {
        has_end_nl = false;
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            if (line[len - 1] == '\r')
                has_cr = true;
            has_end_nl = true;
            len--;
        }
        editorInsertRow(file, at, line, len);
        at++;
    }

    file->lineno_width = getDigit(file->num_rows) + 2;

    if (has_end_nl) {
        editorInsertRow(file, file->num_rows, "", 0);
    }

    if (file->num_rows < 2) {
        file->newline = editorGetDefaultNewline();
    } else if (has_cr) {
        file->newline = NL_DOS;
    } else {
        file->newline = NL_UNIX;
    }

    free(line);
}

OpenStatus editorLoadFile(EditorFile* file, const char* path, bool reload) {
    editorInitFile(file);

    if (path[0] == '\0') {
        editorMsg("Can't open empty path.");
        return OPEN_FAILED;
    }

    FileType type = getFileType(path);
    switch (type) {
        case FT_REG: {
            FileInfo file_info = getFileInfo(path);
            if (file_info.error) {
                editorMsg("Can't open \"%s\"! Failed to get file info.", path);
                return OPEN_FAILED;
            }
            file->has_file_info = true;
            file->file_info = file_info;
            int open_index = isFileOpened(file_info);

            if (open_index != -1 && !reload) {
                int tab_index = editorFindTabByFileIndex(
                    gEditor.split_active_index, open_index);
                if (tab_index != -1) {
                    editorChangeToFile(gEditor.split_active_index, tab_index);
                } else {
                    editorAddTab(gEditor.split_active_index, open_index);
                }
                return OPEN_OPENED;
            }
        } break;

        case FT_DIR:
            if (gEditor.explorer.node) {
                editorExplorerFreeNode(gEditor.explorer.node);
            }
            changeDir(path);
            gEditor.explorer.node = editorExplorerCreate(".");
            gEditor.explorer.node->is_open = true;
            editorExplorerRefresh();

            gEditor.explorer.offset = 0;
            gEditor.explorer.selected_index = 0;
            return OPEN_DIR;

        case FT_ACCESS_DENIED:
            editorMsg("Can't open \"%s\"! Permission denied.", path);
            return OPEN_FAILED;

        case FT_NOT_REG:
            editorMsg("Can't open \"%s\"! Not a regular file.", path);
            return OPEN_FAILED;

        case FT_INVALID:
            editorMsg("Can't open \"%s\"! Invalid path.", path);
            return OPEN_FAILED;

        case FT_NOT_EXIST:
            break;
    }

    FILE* fp = openFile(path, "rb");
    if (!fp) {
        if (errno != ENOENT) {
            editorMsg("Can't open \"%s\"! %s", path, strerror(errno));
            return OPEN_FAILED;
        }

        // file doesn't exist
        char parent_dir[EDITOR_PATH_MAX];
        snprintf(parent_dir, sizeof(parent_dir), "%s", path);
        getDirName(parent_dir);

        if (!pathExists(parent_dir)) {
            editorMsg("Can't open \"%s\"! Directory \"%s\" does not exist.",
                      path, parent_dir);
            return OPEN_FAILED;
        }
    }

    const char* full_path = getFullPath(path);
    size_t path_len = strlen(full_path) + 1;
    free(file->filename);
    file->filename = malloc_s(path_len);
    memcpy(file->filename, full_path, path_len);

    editorSelectSyntaxHighlight(file);

    file->dirty = 0;
    file->read_only = CONVAR_GETINT(readonly) || !canWriteFile(file->filename);

    if (!fp) {
        editorInsertRow(file, 0, "", 0);
        return OPEN_FILE_NEW;
    }

    editorLoadRowsFromStream(file, fp);
    fclose(fp);

    return OPEN_FILE;
}

bool editorSave(EditorFile* file, int save_as) {
    if (!file->filename || save_as) {
        char prompt_buf[64];
        const char* prompt;
        if (file->filename) {
            prompt = "Save as: %s";
        } else {
            snprintf(prompt_buf, sizeof(prompt_buf), "Save Untitled-%d as: %%s",
                     file->new_id + 1);
            prompt = prompt_buf;
        }

        char* path = editorPrompt(prompt, STATE_SAVE_AS_PROMPT, NULL);
        if (!path) {
            editorMsg("Save canceled.");
            return false;
        }

        // Check path is valid
        FILE* fp = openFile(path, "wb");
        if (!fp) {
            editorMsg("Can't save \"%s\"! %s", path, strerror(errno));
            return false;
        }
        fclose(fp);

        const char* full_path = getFullPath(path);
        size_t path_len = strlen(full_path) + 1;
        free(file->filename);
        file->filename = malloc_s(path_len);
        memcpy(file->filename, full_path, path_len);
        free(path);

        editorSelectSyntaxHighlight(file);
    }

    size_t len;
    char* buf = editroRowsToString(file, &len);

    OsError err;
    if (shouldSaveInPlace(file->filename)) {
        err = saveFileInPlace(file->filename, buf, len);
    } else {
        err = saveFileReplace(file->filename, buf, len);
        if (err) {
            err = saveFileInPlace(file->filename, buf, len);
        }
    }

    free(buf);

    if (err) {
        char msg[256];
        formatOsError(err, msg, sizeof(msg));
        editorMsg("Can't save \"%s\"! %s", file->filename, msg);
        editorMsg("Use Alt+A to save as a different file.");
        return false;
    }

    file->dirty = 0;
    editorMsg("%d bytes written to disk.", len);

    // Since we save by replacing the file, we need to refresh file info
    FileInfo file_info = getFileInfo(file->filename);
    if (!file_info.error) {
        file->file_info = file_info;
    }

    return true;
}

bool editorIsDangerousSave(const EditorFile* file, bool verbose) {
    if (!file->has_file_info)
        return false;

    FileInfo new_info = getFileInfo(file->filename);
    if (new_info.error) {
        // File probably removed
        return false;
    }

    if (isFileModified(new_info, file->file_info)) {
        if (verbose) {
            editorMsg("File modified by other program since open.");
        }
        return true;
    }

    if (!canWriteFile(file->filename)) {
        if (verbose) {
            editorMsg("File is read-only on disk.");
        }
        return true;
    }

    return false;
}

static int findAvailableUntitledId(void) {
    for (int id = 0;; id++) {
        int used = 0;
        for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
            const EditorFile* open_file = &gEditor.files[i];
            if (open_file->reference_count == 0)
                continue;
            if (!open_file->filename && open_file->new_id == id) {
                used = 1;
                break;
            }
        }
        if (!used) {
            return id;
        }
    }
}

void editorNewUntitledFile(EditorFile* file) {
    editorInitFile(file);
    editorInsertRow(file, 0, "", 0);
    file->new_id = findAvailableUntitledId();
}

void editorNewUntitledFileFromStdin(EditorFile* file) {
    editorInitFile(file);
    file->new_id = findAvailableUntitledId();
    editorLoadRowsFromStream(file, stdin);

    bool file_empty = (file->num_rows == 1 && file->row[0].size == 0);
    if (!file_empty) {
        // Mark dirty since content is from stdin and not saved yet
        file->dirty = 1;
    }
}

void editorOpenFilePrompt(void) {
    char* path = editorPrompt("Open: %s", STATE_OPEN_PROMPT, NULL);
    if (!path)
        return;

    EditorFile file;
    OpenStatus result = editorLoadFile(&file, path, false);
    if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
        if (editorAddFileToActiveSplit(&file) != -1) {
            gEditor.state = STATE_EDIT;
        }
    } else if (result == OPEN_OPENED) {
        gEditor.state = STATE_EDIT;
    } else if (result == OPEN_DIR) {
        gEditor.state = STATE_EXPLORER;
        editorExplorerShow();
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
            memmove(&data->nodes[i + 1], &data->nodes[i],
                    (data->count - i) * sizeof(EditorExplorerNode*));
            break;
        }
    }

    data->nodes[i] = node;
    data->count++;
}

EditorExplorerNode* editorExplorerCreate(const char* path) {
    EditorExplorerNode* node = malloc_s(sizeof(EditorExplorerNode));

    int len = strlen(path);
    node->filename = malloc_s(len + 1);
    snprintf(node->filename, len + 1, "%s", path);

    node->is_directory = (getFileType(path) == FT_DIR);
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
        const char* filename = dirGetName(&iter);
        if (CONVAR_GETINT(ex_show_hidden) == 0 && filename[0] == '.')
            continue;
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
            continue;

        char entry_path[EDITOR_PATH_MAX];
        snprintf(entry_path, sizeof(entry_path), PATH_CAT("%s", "%s"),
                 node->filename, filename);

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
    gEditor.explorer.node = NULL;
    gEditor.explorer.flatten.data = NULL;
    gEditor.explorer.flatten.size = 0;
    gEditor.explorer.flatten.capacity = 0;
}
