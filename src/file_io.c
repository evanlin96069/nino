#include "file_io.h"

#include <errno.h>
#include <fcntl.h>

#include "config.h"
#include "console.h"
#include "editor.h"
#include "highlight.h"
#include "row.h"

#include "panels/edit.h"
#include "panels/explorer.h"
#include "panels/prompt.h"

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

static char* editorRowsToString(EditorFile* file, size_t* len) {
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
                    gEditor.active_edit_panel, open_index);
                if (tab_index != -1) {
                    editorChangeToFile(gEditor.active_edit_panel, tab_index);
                } else {
                    editorAddTab(gEditor.active_edit_panel, open_index);
                }
                return OPEN_OPENED;
            }
        } break;

        case FT_DIR:
            if (gEditor.explorer_panel->node) {
                editorExplorerFreeNode(gEditor.explorer_panel->node);
            }
            changeDir(path);
            gEditor.explorer_panel->node = editorExplorerCreate(".");
            gEditor.explorer_panel->node->is_open = true;
            editorExplorerRefresh();

            gEditor.explorer_panel->offset = 0;
            gEditor.explorer_panel->selected_index = 0;
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
    file->read_only = readonly.int_value || !canWriteFile(file->filename);

    if (!fp) {
        editorInsertRow(file, 0, "", 0);
        return OPEN_FILE_NEW;
    }

    editorLoadRowsFromStream(file, fp);
    fclose(fp);

    return OPEN_FILE;
}

bool editorSave(EditorFile* file, const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    size_t len;
    char* buf = editorRowsToString(file, &len);

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

static void saveAsCallback(PromptEvent event, void* user_data) {
    UNUSED(user_data);
    if (event.type == PROMPT_EVENT_SUBMIT) {
        EditorFile* file = (EditorFile*)user_data;
        const char* path = event.query;

        // Check path is valid
        FILE* fp = openFile(path, "wb");
        if (!fp) {
            editorMsg("Can't save \"%s\"! %s", path, strerror(errno));
            return;
        }
        fclose(fp);

        // TODO: Check if save overwrites existing file

        const char* full_path = getFullPath(path);
        size_t path_len = strlen(full_path) + 1;
        free(file->filename);
        file->filename = malloc_s(path_len);
        memcpy(file->filename, full_path, path_len);

        editorSelectSyntaxHighlight(file);

        editorSave(file, file->filename);
        editorHelpRestoreMsg();
    } else if (event.type == PROMPT_EVENT_CANCEL) {
        editorMsg("Save canceled.");
        editorHelpRestoreMsg();
    }
}

void editorPromptSaveAs(EditorFile* file) {
    static char prompt_buf[64];
    if (file->filename) {
        snprintf(prompt_buf, sizeof(prompt_buf),
                 "Save %s as: ", getBaseName(file->filename));
    } else {
        snprintf(prompt_buf, sizeof(prompt_buf),
                 "Save Untitled-%d as: ", file->new_id + 1);
    }

    editorHelpSetMsg(HELP_SAVE_AS_PROMPT);
    editorPrompt(prompt_buf, saveAsCallback, file);
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
    for (int id = 0; id < EDITOR_FILE_MAX_SLOT; id++) {
        bool used = false;
        for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
            const EditorFile* open_file = &gEditor.files[i];
            if (open_file->reference_count == 0)
                continue;
            if (!open_file->filename && open_file->new_id == id) {
                used = true;
                break;
            }
        }
        if (!used) {
            return id;
        }
    }

    return -1;
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

static void fileOpenCallback(PromptEvent event, void* user_data) {
    UNUSED(user_data);
    if (event.type == PROMPT_EVENT_SUBMIT) {
        EditorFile file;
        OpenStatus result = editorLoadFile(&file, event.query, false);
        if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
            if (editorAddFileToActiveSplit(&file) != -1) {
                // TODO: focus the tab that has the file opened
            }
        } else if (result == OPEN_OPENED) {
            // TODO: focus the tab that has the file opened
        } else if (result == OPEN_DIR) {
            if (!panelIsEnabled((Panel*)gEditor.explorer_panel))
                uiPanelSetEnabled(&gEditor.ui, (Panel*)gEditor.explorer_panel,
                                  true);
            uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.explorer_panel);
        }
        editorHelpRestoreMsg();
    } else if (event.type == PROMPT_EVENT_CANCEL) {
        editorHelpRestoreMsg();
    }
}

void editorPromptFileOpen(void) {
    editorHelpSetMsg(HELP_OPEN_PROMPT);
    editorPrompt("Open: ", fileOpenCallback, NULL);
}
