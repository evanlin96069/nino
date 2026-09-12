#include "panels/explorer.h"

#include "console.h"
#include "editor.h"

#include "panels/edit.h"

#define EXPLORER_SCROLL_STEP 3

static void destroy(Panel* self);
static void render(Panel* self, Surface s);
static bool getCursor(Panel* self, UICursor* out);
static void onFocus(Panel* self, bool focused);
static void keyEvent(Panel* self, KeyEvent event);
static bool mouseEvent(Panel* self, UIMouseEvent event);

static PanelVtable panel_vt = {
    .destroy = destroy,
    .render = render,
    .getCursor = getCursor,
    .onFocus = onFocus,
    .keyEvent = keyEvent,
    .mouseEvent = mouseEvent,
};

ExplorerPanel* panelExplorerCreate(void) {
    ExplorerPanel* p = calloc_s(1, sizeof(ExplorerPanel));
    p->base.vt = &panel_vt;
    p->base.kind = PANEL_KIND_EXPLORER;
    return p;
}

static void editorExplorerFreeNode(EditorExplorerNode* node);

static void destroy(Panel* self) {
    ExplorerPanel* p = (ExplorerPanel*)self;

    editorExplorerFreeNode(p->node);
    vector_free(p->flatten);
    p->node = NULL;
}

static void render(Panel* self, Surface s) {
    ExplorerPanel* p = (ExplorerPanel*)self;
    bool focused = (gEditor.ui.focused_panel == self);

    if (s.h <= 0 || s.w <= 0) {
        return;
    }

    // Draw header
    ScreenCell* header_row = SURFACE_ROW(s, 0);
    const ScreenStyle header_style = {
        .fg = gEditor.color_cfg[UI_COLOR_EXPLORER_FILE],
        .bg = focused ? gEditor.color_cfg[UI_COLOR_EXPLORER_FOCUS]
                      : gEditor.color_cfg[UI_COLOR_EXPLORER_BG],
    };
    screenClearCells(header_row, s.w, 0, s.w, header_style);
    screenPutAscii(header_row, s.w, 0, " EXPLORER", header_style);

    // Draw tree
    int content_h = s.h - 1;  // row 0 is the header
    int lines = p->flatten.size - p->offset;
    if (lines < 0) {
        lines = 0;
    } else if (lines > content_h) {
        lines = content_h;
    }

    const ScreenStyle default_style = {
        .fg = gEditor.color_cfg[UI_COLOR_EXPLORER_FILE],
        .bg = gEditor.color_cfg[UI_COLOR_EXPLORER_BG],
    };
    const ScreenStyle directory_style = {
        .fg = gEditor.color_cfg[UI_COLOR_EXPLORER_DIRECTORY],
        .bg = gEditor.color_cfg[UI_COLOR_EXPLORER_BG],
    };

    for (int i = 0; i < lines; i++) {
        ScreenCell* row = SURFACE_ROW(s, i + 1);
        int index = p->offset + i;
        EditorExplorerNode* node = p->flatten.data[index];

        ScreenStyle row_style =
            (node->is_directory) ? directory_style : default_style;
        if (index == p->selected_index) {
            row_style.bg = gEditor.color_cfg[UI_COLOR_EXPLORER_SELECT];
        }

        screenClearCells(row, s.w, 0, s.w, row_style);

        // Indentation
        int x = node->depth * 2;

        if (node->is_directory) {
            const char* icon = node->is_open ? "v " : "> ";
            x += screenPutAscii(row, s.w, x, icon, row_style);
        }

        const char* filename = getBaseName(node->filename);
        screenPutUtf8(row, s.w, x, filename, row_style);
    }

    // Draw blank lines
    for (int i = 0; i < content_h - lines; i++) {
        ScreenCell* row = SURFACE_ROW(s, lines + i + 1);
        screenClearCells(row, s.w, 0, s.w, default_style);
    }
}

static bool getCursor(Panel* self, UICursor* out) {
    UNUSED(self);
    UNUSED(out);
    return false;
}

static void onFocus(Panel* self, bool focused) {
    UNUSED(self);
    if (focused) {
        editorHelpSetMsg(HELP_GLOBAL);
    }
}

static void editorExplorerScroll(ExplorerPanel* p, int dist) {
    if (dist == 0)
        return;

    if (dist > 0) {
        // Scroll down
        int h = p->base.layout->rect.h - 1;  // -1 for header
        if ((int)p->flatten.size - p->offset > h) {
            p->offset += dist;
        }
    } else {
        // Scroll up
        if (p->offset > 0) {
            p->offset = (p->offset + dist) > 0 ? (p->offset + dist) : 0;
        }
    }
}

static void editorExplorerScrollToSelected(ExplorerPanel* p) {
    int rows = p->base.layout->rect.h - 1;  // -1 for header
    if (p->offset > p->selected_index) {
        p->offset = p->selected_index;
    } else if ((int)p->selected_index >= p->offset + rows) {
        p->offset = p->selected_index - rows + 1;
    }

    if (p->offset < 0) {
        p->offset = 0;
    }
}

static void editorExplorerOpenSelected(ExplorerPanel* p) {
    EditorExplorerNode* node = NULL;
    if (p->selected_index < (int)p->flatten.size)
        node = p->flatten.data[p->selected_index];

    if (!node)
        return;

    if (node->is_directory) {
        node->is_open ^= 1;
        editorExplorerRefresh();
    } else {
        EditorFile file = {0};
        EditorOpenStatus result = editorLoadFile(&file, node->filename, false);
        if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
            editorAddFileToActiveSplit(&file);
        }
    }
}

static void keyEvent(Panel* self, KeyEvent event) {
    ExplorerPanel* p = (ExplorerPanel*)self;
    switch (event.value) {
        case KEYVAL(KEY_TEXT): {
            if (!p->node)
                return;

            uint32_t unicode = event.unicode;
            if (unicode > 255)
                return;

            char c = toLower(unicode);
            size_t index = p->selected_index + 1;
            for (size_t i = 0; i < p->flatten.size; i++) {
                index = index % p->flatten.size;
                if (toLower(getBaseName(p->flatten.data[index]->filename)[0]) ==
                    c) {
                    p->selected_index = index;
                    editorExplorerScrollToSelected(p);
                    break;
                }
                index++;
            }
        } break;

        case KEYVAL(KEY_UP):
            if (p->selected_index <= 0)
                break;
            p->selected_index--;
            editorExplorerScrollToSelected(p);
            break;

        case KEYVAL(KEY_DOWN):
            if (p->selected_index + 1 >= (int)p->flatten.size)
                break;
            p->selected_index++;
            editorExplorerScrollToSelected(p);
            break;

        case KEYVAL(KEY_HOME):
            p->selected_index = 0;
            editorExplorerScrollToSelected(p);
            break;

        case KEYVAL(KEY_END):
            p->selected_index = p->flatten.size - 1;
            editorExplorerScrollToSelected(p);
            break;

        case KEYVAL(KEY_PAGE_UP): {
            int rows = p->base.layout->rect.h - 1;  // -1 for header
            if (p->selected_index != p->offset) {
                p->selected_index = p->offset;
            } else {
                p->selected_index -= rows;
                if (p->selected_index < 0) {
                    p->selected_index = 0;
                }
            }
            editorExplorerScrollToSelected(p);
        } break;

        case KEYVAL(KEY_PAGE_DOWN): {
            int rows = p->base.layout->rect.h - 1;  // -1 for header
            if (p->selected_index != p->offset + rows - 1) {
                p->selected_index = p->offset + rows - 1;
            } else {
                p->selected_index += rows;
            }

            if (p->selected_index >= (int)p->flatten.size) {
                p->selected_index = p->flatten.size - 1;
            }
            editorExplorerScrollToSelected(p);
        } break;

        case KEYVAL(KEY_ENTER):
            editorExplorerOpenSelected(p);
            break;

        default:
            break;
    }
}

static bool mouseEvent(Panel* self, UIMouseEvent event) {
    ExplorerPanel* p = (ExplorerPanel*)self;
    switch (event.mouse.type) {
        case MOUSE1_PRESSED: {
            int y = event.mouse.y;
            if (y < 1 || y > (int)p->flatten.size - p->offset)
                break;

            p->selected_index = y - 1 + p->offset;
            editorExplorerOpenSelected(p);
        } break;

        case MWHEEL_UP:
            editorExplorerScroll(gEditor.explorer_panel, -EXPLORER_SCROLL_STEP);
            break;

        case MWHEEL_DOWN:
            editorExplorerScroll(gEditor.explorer_panel, EXPLORER_SCROLL_STEP);
            break;

        default:
            break;
    }
    return false;
}

static EditorExplorerNode* editorExplorerCreate(const char* path,
                                                bool is_directory) {
    EditorExplorerNode* node = calloc_s(1, sizeof(EditorExplorerNode));

    size_t path_len = strlen(path) + 1;
    node->filename = malloc_s(path_len);
    memcpy(node->filename, path, path_len);

    node->is_directory = is_directory;

    return node;
}

static inline void editorExplorerFreeNodes(VecEditorExplorerNode* nodes) {
    for (size_t i = 0; i < nodes->size; i++) {
        editorExplorerFreeNode(nodes->data[i]);
    }
    vector_free(*nodes);
}

static void editorExplorerFreeNode(EditorExplorerNode* node) {
    if (!node)
        return;

    if (node->is_directory) {
        editorExplorerFreeNodes(&node->dir_nodes);
        editorExplorerFreeNodes(&node->file_nodes);
    }

    free(node->filename);
    free(node);
}

void editorExplorerOpenDir(const char* path) {
    ExplorerPanel* p = gEditor.explorer_panel;

    const char* full_path = getFullPath(path);
    if (!full_path) {
        editorMsg("Can't resolve path \"%s\"!", path);
        return;
    }

    if (p->node) {
        editorExplorerFreeNode(p->node);
    }

    p->node = editorExplorerCreate(full_path, true);
    p->node->is_open = true;

    editorExplorerRefresh();

    p->offset = 0;
    p->selected_index = 0;
}

// Insert in dictionary order
static void editorExplorerInsertNode(VecEditorExplorerNode* nodes,
                                     EditorExplorerNode* child) {
    size_t i;
    for (i = 0; i < nodes->size; i++) {
        if (strcmp(nodes->data[i]->filename, child->filename) > 0) {
            break;
        }
    }

    vector_insert(*nodes, i, child);
}

static bool editorExplorerRescanNode(EditorExplorerNode* node, bool force) {
    if (!node->is_directory)
        return false;

    FileInfo info = getFileInfo(node->filename);
    if (node->loaded) {
        if (!force && !isFileModified(info, node->info)) {
            return true;
        }
    } else {
        vector_clear(node->dir_nodes);
        vector_clear(node->file_nodes);
    }

    DirIter iter = dirFindFirst(node->filename);
    if (iter.error) {
        // Dir likely not exist anymore
        vector_free(node->dir_nodes);
        vector_free(node->file_nodes);
        return false;
    }

    VecEditorExplorerNode old_dir_nodes = node->dir_nodes;
    VecEditorExplorerNode old_file_nodes = node->file_nodes;

    VecEditorExplorerNode dir_nodes = {0};
    VecEditorExplorerNode file_nodes = {0};

    do {
        const char* filename = dirGetName(&iter);
        if (ex_show_hidden.int_value == 0 && filename[0] == '.')
            continue;
        if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0)
            continue;

        char entry_path[EDITOR_PATH_MAX];
        snprintf(entry_path, sizeof(entry_path), PATH_CAT("%s", "%s"),
                 node->filename, filename);

        bool is_directory = (getFileType(entry_path) == FT_DIR);

        VecEditorExplorerNode* old_nodes =
            is_directory ? &old_dir_nodes : &old_file_nodes;
        VecEditorExplorerNode* new_nodes =
            is_directory ? &dir_nodes : &file_nodes;

        // Find existing node
        EditorExplorerNode* child = NULL;
        for (size_t i = 0; i < old_nodes->size; i++) {
            EditorExplorerNode* curr_node = old_nodes->data[i];
            if (strcmp(curr_node->filename, entry_path) == 0) {
                vector_erase(*old_nodes, i);
                child = curr_node;
                break;
            }
        }

        // Not exist, create new
        if (!child) {
            child = editorExplorerCreate(entry_path, is_directory);
            child->depth = node->depth + 1;
        }
        editorExplorerInsertNode(new_nodes, child);
    } while (dirNext(&iter));
    dirClose(&iter);

    node->dir_nodes = dir_nodes;
    node->file_nodes = file_nodes;

    // Cleanup nodes no longer on disk
    editorExplorerFreeNodes(&old_dir_nodes);
    editorExplorerFreeNodes(&old_file_nodes);

    node->info = info;

    return true;
}

static void editorExplorerFlattenNode(EditorExplorerNode* node, bool reload, bool force) {
    if (!node)
        return;

    if (node != gEditor.explorer_panel->node)
        vector_push(gEditor.explorer_panel->flatten, node);

    if (node->is_directory && node->is_open) {
        if (reload || !node->loaded) {
            node->loaded = editorExplorerRescanNode(node, force);
        }

        for (size_t i = 0; i < node->dir_nodes.size; i++) {
            editorExplorerFlattenNode(node->dir_nodes.data[i], reload, force);
        }

        for (size_t i = 0; i < node->file_nodes.size; i++) {
            editorExplorerFlattenNode(node->file_nodes.data[i], reload, force);
        }
    }
}

void editorExplorerRefresh(void) {
    vector_clear(gEditor.explorer_panel->flatten);
    editorExplorerFlattenNode(gEditor.explorer_panel->node, false, false);
}

void editorExplorerReload(bool force) {
    ExplorerPanel* p = gEditor.explorer_panel;
    if (!p->node)
        return;

    char selected_path[EDITOR_PATH_MAX] = "";
    if (p->selected_index >= 0 && p->selected_index < (int)p->flatten.size) {
        snprintf(selected_path, sizeof(selected_path), "%s",
                 p->flatten.data[p->selected_index]->filename);
    }

    vector_clear(gEditor.explorer_panel->flatten);
    editorExplorerFlattenNode(gEditor.explorer_panel->node, true, force);

    int new_index = -1;
    if (selected_path[0]) {
        for (size_t i = 0; i < p->flatten.size; i++) {
            if (strcmp(p->flatten.data[i]->filename, selected_path) == 0) {
                new_index = (int)i;
                break;
            }
        }
    }

    if (new_index >= 0) {
        p->selected_index = new_index;

        if (p->offset > (int)p->flatten.size - 1) {
            p->offset = (int)p->flatten.size - 1;
        }
        if (p->offset < 0) {
            p->offset = 0;
        }
    } else {
        p->offset = 0;
        p->selected_index = 0;
    }
}

void editorExplorerSetSide(bool left) {
    LayoutNode* node = gEditor.explorer_panel->base.layout;
    LayoutNode* parent = gEditor.explorer_panel->base.layout->parent;
    if (!parent)
        return;

    uiDetachPanel(&gEditor.ui, (Panel*)gEditor.explorer_panel);

    if (left) {
        layoutInsertChild(parent, 0, node);
    } else {
        layoutInsertChild(parent, parent->children.size, node);
    }

    layoutUpdate(gEditor.ui.root);
}
