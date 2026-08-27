#include "panels/explorer.h"

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
        OpenStatus result = editorLoadFile(&file, node->filename, false);
        if (result == OPEN_FILE || result == OPEN_FILE_NEW) {
            editorAddFileToActiveSplit(&file);
        }
    }
}

static void keyEvent(Panel* self, KeyEvent event) {
    ExplorerPanel* p = (ExplorerPanel*)self;
    switch (event.value) {
        case KEY_EVENT(KEY_TEXT): {
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

        case KEY_EVENT(KEY_UP):
            if (p->selected_index <= 0)
                break;
            p->selected_index--;
            editorExplorerScrollToSelected(p);
            break;

        case KEY_EVENT(KEY_DOWN):
            if (p->selected_index + 1 >= (int)p->flatten.size)
                break;
            p->selected_index++;
            editorExplorerScrollToSelected(p);
            break;

        case KEY_EVENT(KEY_HOME):
            p->selected_index = 0;
            editorExplorerScrollToSelected(p);
            break;

        case KEY_EVENT(KEY_END):
            p->selected_index = p->flatten.size - 1;
            editorExplorerScrollToSelected(p);
            break;

        case KEY_EVENT(KEY_PAGE_UP): {
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

        case KEY_EVENT(KEY_PAGE_DOWN): {
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

        case KEY_EVENT(KEY_ENTER):
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

void editorExplorerFreeNode(EditorExplorerNode* node) {
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

static void insertNode(EditorExplorerNode* node, EditorExplorerNodeData* data) {
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

static void loadNode(EditorExplorerNode* node) {
    if (!node->is_directory)
        return;

    DirIter iter = dirFindFirst(node->filename);
    if (iter.error)
        return;

    do {
        const char* filename = dirGetName(&iter);
        if (ex_show_hidden.int_value == 0 && filename[0] == '.')
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
            insertNode(child, &node->dir);
        } else {
            insertNode(child, &node->file);
        }
    } while (dirNext(&iter));
    dirClose(&iter);

    node->loaded = true;
}

static void flattenNode(EditorExplorerNode* node) {
    if (!node)
        return;

    if (node != gEditor.explorer_panel->node)
        vector_push(gEditor.explorer_panel->flatten, node);

    if (node->is_directory && node->is_open) {
        if (!node->loaded)
            loadNode(node);

        for (size_t i = 0; i < node->dir.count; i++) {
            flattenNode(node->dir.nodes[i]);
        }

        for (size_t i = 0; i < node->file.count; i++) {
            flattenNode(node->file.nodes[i]);
        }
    }
}

void editorExplorerRefresh(void) {
    vector_clear(gEditor.explorer_panel->flatten);
    flattenNode(gEditor.explorer_panel->node);
}
