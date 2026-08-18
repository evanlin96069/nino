#include "input.h"

#include "config.h"
#include "console.h"
#include "editor.h"
#include "file_io.h"
#include "select.h"
#include "terminal.h"

#include "panels/edit.h"
#include "ui/compositor.h"

static bool globalKeyEvent(Panel* panel, EditorInput input) {
    if (gEditor.ui.focused_panel == (Panel*)gEditor.prompt_panel) {
        // Let the prompt handle the input
        return false;
    }

    bool pending_quit_confirm = gEditor.pending_quit_confirm;
    gEditor.pending_quit_confirm = false;

    bool handled = true;

    switch (input.type) {
        // Quit
        case CTRL_KEY('q'): {
            if (pending_quit_confirm) {
                gEditor.state = STATE_EXIT;
                break;
            }

            int dirty_count = editorGetDirtyFileCount();
            if (dirty_count > 0) {
                gEditor.pending_quit_confirm = true;

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

        // Prompt
        case CTRL_KEY('p'):
            editorPromptConfig();
            gEditor.con_keep_msg = true;
            break;

        // Open file
        case CTRL_KEY('o'):
            editorPromptFileOpen();
            break;

        // New tab
        case CTRL_KEY('n'): {
            EditorFile new_file;
            editorNewUntitledFile(&new_file);
            if (editorAddFileToActiveSplit(&new_file) != -1) {
                uiPanelSetFocused(&gEditor.ui,
                                  (Panel*)gEditor.active_edit_panel);
            }
        } break;

        // Toggle explorer
        case CTRL_KEY('b'):
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
        case CTRL_KEY('e'):
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
        case CTRL_ALT_LEFT:
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_LEFT);
            break;

        case CTRL_ALT_RIGHT:
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_RIGHT);
            break;

        case CTRL_ALT_UP:
            uiPanelNavigate(&gEditor.ui, LAYOUT_DIR_UP);
            break;

        case CTRL_ALT_DOWN:
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
            gEditor.pending_edit_panel = NULL;
        }
    }

    return handled;
}

static bool globalMouseEvent(Panel* panel, UIMouseEvent mouse_event) {
    switch (mouse_event.state->type) {
        case UI_MOUSE1_PRESSED:
        case UI_MWHEEL_UP:
        case UI_MWHEEL_DOWN:
            // Only clear console for these events
            break;

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
            gEditor.pending_edit_panel = NULL;
        }
    }

    return false;
}

UIProcessInputHooks global_input_hooks = {
    .preKeyEvent = globalKeyEvent,
    .preMouseEvent = globalMouseEvent,
};

void editorProcessInput(void) {
    EditorInput input = editorReadKey();  // TODO: Add record/replay feature
    if (input.type != UNKNOWN) {
        uiProcessInput(&gEditor.ui, input, global_input_hooks);
    }
    editorFreeInput(&input);
}
