#include "input.h"

#include "config.h"
#include "console.h"
#include "editor.h"
#include "file_io.h"
#include "select.h"
#include "terminal.h"

#include "panels/edit.h"
#include "ui/compositor.h"

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

void editorProcessInput(void) {
    // TODO: Add record/replay feature
    Event event = eventPoll(READ_WAIT_INFINITE);
    switch (event.type) {
        case EVENT_KEY:
            uiProcessKeyEvent(&gEditor.ui, event.key, preKeyEvent);
            break;

        case EVENT_MOUSE:
            uiProcessMouseEvent(&gEditor.ui, event.mouse, getTimeMs(),
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
            editorSetWindowSize(event.resize.rows, event.resize.cols);
            break;

        default:
            break;
    }

    eventFree(&event);
}
