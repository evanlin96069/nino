#include "editor.h"

#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "highlight.h"

#include "utils/os.h"
#include "utils/unicode.h"
#include "utils/utils.h"

#include "panels/edit.h"
#include "panels/explorer.h"
#include "panels/prompt.h"
#include "panels/welcome.h"

// Editor

Editor gEditor;

static void editorInitUI(void) {
    uiInit(&gEditor.ui);

    gEditor.ui.root = layoutNodeCreate(LAYOUT_TOPBOTTOM);
    gEditor.ui.root->resizable = false;

    // Root top
    LayoutNode* top_node = layoutNodeCreate(LAYOUT_LEFTRIGHT);

    ExplorerPanel* explorer_panel = panelExplorerCreate();
    LayoutNode* explorer_node = layoutNodeCreateLeaf((Panel*)explorer_panel);
    explorer_node->size_type = LAYOUT_SIZE_FIXED;
    explorer_node->fixed_size = ex_default_width.int_value;
    explorer_node->enabled = false;

    WelcomePanel* welcome_panel = panelWelcomeCreate();
    LayoutNode* welcome_node = layoutNodeCreateLeaf((Panel*)welcome_panel);

    layoutAppendChild(top_node, explorer_node);
    layoutAppendChild(top_node, welcome_node);
    // Edit panels will be created on file open

    // Root bottom
    PromptPanel* prompt_panel = panelPromptCreate();
    LayoutNode* prompt_node = layoutNodeCreateLeaf((Panel*)prompt_panel);
    prompt_node->size_type = LAYOUT_SIZE_FIXED;
    prompt_node->fixed_size = 1;
    prompt_node->resizable = false;
    prompt_node->enabled = false;

    layoutAppendChild(gEditor.ui.root, top_node);
    layoutAppendChild(gEditor.ui.root, prompt_node);

    layoutUpdate(gEditor.ui.root);

    gEditor.explorer_panel = explorer_panel;
    gEditor.welcome_panel = welcome_panel;
    gEditor.prompt_panel = prompt_panel;
    gEditor.active_edit_panel = NULL;
}

void editorInit(void) {
    gEditor.state = STATE_LOADING;
    gEditor.mouse_mode = true;
    memcpy(gEditor.color_cfg, color_default, sizeof(gEditor.color_cfg));
    gEditor.con_front = -1;

    editorRegisterCommands();
    editorInitHLDB();

    editorInitUI();
}

void editorFree(void) {
    uiFree(&gEditor.ui);

#ifndef NDEBUG
    // Check if any files are somehow not associated with any splits
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count > 0) {
            PANIC(
                "File(s) still open after uiFree. This indicates a memory "
                "leak.");
        }
    }
#endif

    vector_free(&gEditor.recent_splits);
    editorFreeClipboardContent(&gEditor.clipboard);
    editorFreeHLDB();
    editorUnregisterCommands();
}

void editorSetWindowSize(int screen_width, int screen_height) {
    if (!gEditor.ui.root)
        return;

    screen_width = screen_width < 1 ? 1 : screen_width;
    screen_height = screen_height < 1 ? 1 : screen_height;
    gEditor.screen_width = screen_width;
    gEditor.screen_height = screen_height;

    // One row for status bar
    gEditor.ui.root->rect = (Rect){0, 0, screen_width, screen_height - 1};
    layoutUpdate(gEditor.ui.root);
}

// IO
static bool preKeyEvent(Panel* panel, KeyEvent event) {
    EditorWaitState wait_state = gEditor.wait_state;
    gEditor.wait_state = EDITOR_WAIT_NONE;

    if (gEditor.ui.focused_panel == (Panel*)gEditor.prompt_panel) {
        // Let the prompt handle the input
        return false;
    }

    bool handled = true;

    switch (event.value) {
        // Quit
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'Q'): {
            // Handle quit confirmation
            if (wait_state == EDITOR_WAIT_QUIT) {
                gEditor.state = STATE_EXIT;
                break;
            }

            int dirty_count = editorGetDirtyFileCount();
            if (dirty_count > 0) {
                gEditor.wait_state = EDITOR_WAIT_QUIT;

                editorMsgClear();
                if (dirty_count == 1) {
                    editorMsg("File has unsaved changes.");
                } else {
                    editorMsg("Files have unsaved changes.");
                }
                editorMsg("Press quit again to quit anyway.");
                gEditor.con_keep_msg = true;
            } else {
                gEditor.state = STATE_EXIT;
            }
        } break;

        // Close tab
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'W'): {
            EditPanel* split = gEditor.active_edit_panel;
            if (!split || split->tab_active_index == -1)
                break;

            // Handle tab close confirmation
            if (wait_state == EDITOR_WAIT_CLOSE) {
                editorCloseTab(split, split->tab_active_index);
                break;
            }

            EditorTab* tab = editorSplitGetTab(split);
            EditorFile* file = editorTabGetFile(tab);
            if (file->dirty && file->reference_count == 1) {
                gEditor.wait_state = EDITOR_WAIT_CLOSE;

                editorMsgClear();
                editorMsg("File has unsaved changes.");
                editorMsg("Press close again to close file anyway.");
                gEditor.con_keep_msg = true;
            } else {
                editorCloseTab(split, split->tab_active_index);
            }
        } break;

        // Prompt
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'P'):
            editorPromptConfig();
            gEditor.con_keep_msg = true;
            break;

        // Open file
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'O'):
            editorPromptFileOpen();
            break;

        // New tab
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'N'): {
            EditorFile new_file;
            editorNewUntitledFile(&new_file);
            if (editorAddFileToActiveSplit(&new_file) != -1) {
                uiPanelSetFocused(&gEditor.ui,
                                  (Panel*)gEditor.active_edit_panel);
            }
        } break;

        // Toggle explorer
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'B'):
            if (panelIsEnabled((Panel*)gEditor.explorer_panel)) {
                uiPanelSetEnabled(&gEditor.ui, (Panel*)gEditor.explorer_panel,
                                  false);
                editorFocusActiveSplit();
            } else {
                uiPanelSetEnabled(&gEditor.ui, (Panel*)gEditor.explorer_panel,
                                  true);
                uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.explorer_panel);
            }
            break;

        // Toggle explorer focus
        case KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'E'):
            if (gEditor.ui.focused_panel == (Panel*)gEditor.explorer_panel) {
                editorFocusActiveSplit();
            } else {
                if (!panelIsEnabled((Panel*)gEditor.explorer_panel)) {
                    uiPanelSetEnabled(&gEditor.ui,
                                      (Panel*)gEditor.explorer_panel, true);
                }
                uiPanelSetFocused(&gEditor.ui, (Panel*)gEditor.explorer_panel);
            }
            break;

        // Navigate panels
        case KEYVAL(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_LEFT):
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_LEFT);
            break;

        case KEYVAL(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_RIGHT):
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_RIGHT);
            break;

        case KEYVAL(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_UP):
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_UP);
            break;

        case KEYVAL(KEY_MOD_CTRL | KEY_MOD_ALT, KEY_DOWN):
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_DOWN);
            break;

        default:
            handled = false;
            break;
    }

    if (!gEditor.con_keep_msg) {
        editorMsgClear();
    } else {
        gEditor.con_keep_msg = false;
    }

    if (gEditor.pending_edit_panel) {
        if (handled || panel != (Panel*)gEditor.pending_edit_panel) {
            editorCancelPendingWait(gEditor.pending_edit_panel);
        }
    }

    return handled;
}

static bool preMouseEvent(Panel* panel, UIMouseEvent event) {
    switch (event.mouse.type) {
        case MOUSE1_PRESSED:
        case MWHEEL_UP:
        case MWHEEL_DOWN:
            // Only clear console for these events
            break;

        case MOUSE2_PRESSED:
        case MOUSE2_RELEASED:
        case MOUSE2_DRAG:
        case MOUSE3_DRAG:
            return true;  // Ignore these events

        default:
            gEditor.con_keep_msg = true;
            break;
    }

    if (!gEditor.con_keep_msg) {
        editorMsgClear();
    } else {
        gEditor.con_keep_msg = false;
    }

    if (gEditor.pending_edit_panel) {
        if (panel != (Panel*)gEditor.pending_edit_panel) {
            editorCancelPendingWait(gEditor.pending_edit_panel);
        }
    }

    return false;
}

void editorProcessEvent(Event event, uint64_t timestamp_ms) {
    switch (event.type) {
        case EVENT_KEY:
            uiProcessKeyEvent(&gEditor.ui, event.key, preKeyEvent);
            break;

        case EVENT_MOUSE:
            uiProcessMouseEvent(&gEditor.ui, event.mouse, timestamp_ms,
                                preMouseEvent);
            break;

        case EVENT_PASTE: {
            EditorClipboard old_clipboard = gEditor.clipboard;
            gEditor.clipboard = event.paste;
            gEditor.is_paste_event = true;
            // Hack
            Event fake_event = {
                .type = EVENT_KEY,
                .key = {.value = KEYVAL(KEY_MOD_CTRL, KEY_CHAR, 'V')},
            };
            uiProcessKeyEvent(&gEditor.ui, fake_event.key, preKeyEvent);
            gEditor.clipboard = old_clipboard;
            gEditor.is_paste_event = false;
        } break;

        case EVENT_RESIZE:
            editorSetWindowSize(event.resize.width, event.resize.height);
            break;

        case EVENT_FOCUS_GAINED:
            if (!autoreload.int_value)
                break;

            for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
                if (gEditor.files[i].reference_count > 0) {
                    editorReloadFile(i, false);
                }
            }
            editorExplorerReload(false);
            break;

        default:
            break;
    }
}

// TODO: Add overlay system
static void editorDrawConMsg(Surface surface) {
    if (gEditor.con_size == 0) {
        return;
    }

    ScreenStyle style = {
        .fg = gEditor.color_cfg[UI_COLOR_PROMPT_FG],
        .bg = gEditor.color_cfg[UI_COLOR_PROMPT_BG],
    };

    // con_size + status bar
    int draw_row = surface.h - (gEditor.con_size + 1);
    if (panelIsEnabled((Panel*)gEditor.prompt_panel)) {
        draw_row--;  // TODO: Adjust for prompt panel height if needed
    }

    int index = gEditor.con_front;
    for (int i = 0; i < gEditor.con_size; i++) {
        ScreenCell* row = SURFACE_ROW(surface, draw_row);
        screenClearCells(row, surface.w, 0, surface.w, style);

        const char* buf = gEditor.con_msg[index];
        index = (index + 1) % EDITOR_CON_COUNT;

        screenPutUtf8(row, surface.w, 0, buf, style);

        draw_row++;
    }
}

// TODO: Move this to somewhere with the status bar
static void editorDrawStatusBar(Surface surface) {
    if (surface.h != 1 || surface.w <= 0) {
        return;
    }

    int w = surface.w;
    ScreenCell* row = surface.cells;
    ScreenStyle default_style = {
        .fg = gEditor.color_cfg[UI_COLOR_STATUS_FG],
        .bg = gEditor.color_cfg[UI_COLOR_STATUS_BG],
    };

    screenClearCells(row, w, 0, w, default_style);

    EditorHelpMsg help_msg = helpinfo.int_value ? gEditor.help_msg : HELP_NONE;
    const char* help_str = editorHelpMsgToString(help_msg);

    char lang[16];
    char pos[64];
    int rlen = 0;

    EditorTab* tab = editorGetActiveTab();
    const EditorFile* file = editorTabGetFile(tab);
    if (file) {
        const char* file_type =
            file->syntax ? file->syntax->file_type : "Plain Text";
        int row_num = tab->cursor.y + 1;
        int col = editorRowCxToRx(&file->row[tab->cursor.y], tab->cursor.x) + 1;
        float line_percent = 0.0f;
        const char* nl_type = (file->newline == NL_UNIX) ? "LF" : "CRLF";
        if (file->num_rows - 1 > 0) {
            line_percent =
                (float)tab->row_offset / (file->num_rows - 1) * 100.0f;
        }

        snprintf(lang, sizeof(lang), "  %s  ", file_type);
        snprintf(pos, sizeof(pos), " %d:%d [%.f%%] <%s> ", row_num, col,
                 line_percent, nl_type);
        rlen = strUTF8Width(lang) + strUTF8Width(pos);
    }

    if (rlen > w)
        rlen = 0;

    int x = 0;
    ScreenStyle style = default_style;

    int max_help_width = (rlen > 0) ? w - rlen : w;
    if (max_help_width > 0) {
        x += screenPutAscii(row, max_help_width, 0, help_str, style);
    }

    if (rlen > 0 && surface.w - x >= rlen) {
        int right_x = surface.w - rlen;
        style.fg = gEditor.color_cfg[UI_COLOR_STATUS_LANG_FG];
        style.bg = gEditor.color_cfg[UI_COLOR_STATUS_LANG_BG];
        int lang_width = strUTF8Width(lang);
        screenPutAscii(row, surface.w, right_x, lang, style);
        style.fg = gEditor.color_cfg[UI_COLOR_STATUS_POS_FG];
        style.bg = gEditor.color_cfg[UI_COLOR_STATUS_POS_BG];
        screenPutAscii(row, surface.w, right_x + lang_width, pos, style);
    }
}

void editorDrawScreen(Surface s, UICursor* out_cursor) {
    if (s.w <= 0 || s.h <= 0) {
        return;
    }

    // One row for status bar
    Rect ui_rect = {0, 0, s.w, s.h - 1};
    Rect status_rect = {0, s.h - 1, s.w, 1};

    // TODO: Make a separate color for separators
    ScreenStyle sep_style = {
        .bg = gEditor.color_cfg[UI_COLOR_BG],
        .fg = gEditor.color_cfg[UI_COLOR_TOP_TABS_FG],
    };
    uiComposite(&gEditor.ui, surfaceSub(s, ui_rect), sep_style);

    // TODO: Refactor these into the new UI API
    editorDrawStatusBar(surfaceSub(s, status_rect));
    editorDrawConMsg(s);

    if (out_cursor) {
        uiGetCursor(&gEditor.ui, out_cursor);
    }
}

// File

void editorInitFile(EditorFile* file) {
    memset(file, 0, sizeof(EditorFile));
    file->newline = editorGetDefaultNewline();
}

void editorFreeFile(EditorFile* file) {
    for (int i = 0; i < file->num_rows; i++) {
        editorFreeRow(&file->row[i]);
    }
    editorFreeActionList(file->action_head);
    free(file->row);
    free(file->filename);
}

int editorAddFile(EditorFile* file) {
    int index = -1;
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count == 0) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        editorMsg("Already opened too many files!");
        editorFreeFile(file);
        return -1;
    }

    EditorFile* current = &gEditor.files[index];

    *current = *file;
    current->action_head = calloc_s(1, sizeof(EditorActionList));
    current->action_current = current->action_head;
    current->reference_count = 0;

    return index;
}

void editorRemoveFile(int file_index) {
    if (file_index < 0 || file_index >= EDITOR_FILE_MAX_SLOT)
        return;

    EditorFile* file = &gEditor.files[file_index];
    if (file->reference_count <= 0) {
        // Likely during the file creation
        if (file->row || file->filename || file->action_head) {
            editorFreeFile(file);
            memset(file, 0, sizeof(EditorFile));
        }
        return;
    }

    file->reference_count--;
    if (file->reference_count == 0) {
        editorFreeFile(file);
        memset(file, 0, sizeof(EditorFile));
        gEditor.file_count--;
    }
}

int editorGetDirtyFileCount(void) {
    int count = 0;
    for (int i = 0; i < EDITOR_FILE_MAX_SLOT; i++) {
        if (gEditor.files[i].reference_count > 0 && gEditor.files[i].dirty) {
            count++;
        }
    }
    return count;
}

// Help
// TODO: Move this to somewhere with the status bar

const char* editorHelpMsgToString(EditorHelpMsg msg) {
    switch (msg) {
        case HELP_GLOBAL:
            if (panelIsEnabled((Panel*)gEditor.welcome_panel) &&
                intro.int_value) {
                // Intro already shows the help message
                return "";
            }
            return " ^Q: Quit  ^O: Open  ^P: Prompt";

        case HELP_EDIT:
            return " ^Q: Quit  ^O: Open  ^P: Prompt  ^S: Save  ^F: Find  ^G: "
                   "Goto";

        case HELP_FIND_PROMPT:
            return " ^Q: Cancel  Up: Back  Down: Next";

        case HELP_GOTO_PROMPT:
        case HELP_OPEN_PROMPT:
        case HELP_CONFIG_PROMPT:
        case HELP_SAVE_AS_PROMPT:
            return " ^Q: Cancel";

        default:
            return "";
    }
}

void editorHelpSetMsg(EditorHelpMsg msg) {
    gEditor.help_msg_prev = gEditor.help_msg;
    gEditor.help_msg = msg;
}

void editorHelpRestoreMsg(void) {
    gEditor.help_msg = gEditor.help_msg_prev;
}
