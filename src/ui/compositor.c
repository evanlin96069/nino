#include "ui/compositor.h"

#include "ui/layout.h"
#include "ui/panel.h"
#include "ui/surface.h"

void uiFree(UI* ui) {
    layoutFree(ui->root);
    ui->root = NULL;
    ui->focused_panel = NULL;
    vector_free(ui->separators);
}

void uiComposite(UI* ui, Surface s, ScreenStyle sep_style) {
    if (s.w <= 0 || s.h <= 0)
        return;

    Rect available = {
        .x = 0,
        .y = 0,
        .w = s.w,
        .h = s.h,
    };
    vector_clear(ui->separators);
    layoutCompute(ui->root, available, &ui->separators);
    layoutRender(ui->root, s);

    // Draw separators
    ScreenCell sep_cell = {
        .continuation = false,
        .grapheme =
            {
                .cluster = {0},
                .size = 1,
                .width = 1,
            },
        .style = sep_style,
    };

    for (uint32_t i = 0; i < ui->separators.size; i++) {
        const Separator* sep = &ui->separators.data[i];
        bool leftright = (sep->parent->kind == LAYOUT_LEFTRIGHT);
        if (leftright) {
            sep_cell.grapheme.cluster[0] = '|';
        } else {
            sep_cell.grapheme.cluster[0] = '-';
        }

        for (int x = sep->rect.x; x < sep->rect.x + sep->rect.w; x++) {
            for (int y = sep->rect.y; y < sep->rect.y + sep->rect.h; y++) {
                SURFACE_AT(s, x, y) = sep_cell;
            }
        }
    }
}

bool uiGetCursor(UI* ui, UICursor* out) {
    Panel* panel = ui->focused_panel;
    if (!panel || !panelIsEnabled(panel) || !panel->vt->getCursor(panel, out)) {
        out->visible = false;
        out->x = 0;
        out->y = 0;
        return false;
    }

    // Make sure the cursor is within the panel
    if (out->x < 0 || out->x >= panel->layout->rect.w || out->y < 0 ||
        out->y >= panel->layout->rect.h) {
        out->visible = false;
        out->x = 0;
        out->y = 0;
        return false;
    }

    rectToGlobal(panel->layout->rect, out->x, out->y, &out->x, &out->y);
    return true;
}

void uiProcessKeyEvent(UI* ui, KeyEvent event, UIPreKeyEvent pre_key_event) {
    if (ui->focused_panel) {
        if (!pre_key_event || !pre_key_event(ui->focused_panel, event)) {
            ui->focused_panel->vt->keyEvent(ui->focused_panel, event);
        }
    }
}

void uiProcessMouseEvent(UI* ui,
                         MouseEvent event,
                         uint64_t timestamp_ms,
                         UIPreMouseEvent pre_mouse_event) {
    int x = event.x;
    int y = event.y;

    switch (event.type) {
        case MOUSE1_PRESSED: {
            int prev_x = ui->mouse.drag.start_x;
            int prev_y = ui->mouse.drag.start_y;
            if (x == prev_x && y == prev_y &&
                timestamp_ms - ui->mouse.last_click_time <
                    UI_MOUSE_DOUBLE_CLICK_TIME) {
                ui->mouse.click_count++;
            } else {
                ui->mouse.click_count = 1;
            }
            ui->mouse.last_click_time = timestamp_ms;

            ui->mouse.drag.start_x = x;
            ui->mouse.drag.start_y = y;

            Separator* sep = layoutFindSeparatorAt(&ui->separators, x, y);
            if (sep) {
                ui->mouse.drag.type = UI_DRAG_SEPARATOR;
                ui->mouse.drag.separator = *sep;
                break;
            }

            LayoutNode* node = layoutFindAt(ui->root, x, y);
            if (!node || node->kind != LAYOUT_LEAF)
                break;

            Panel* panel = node->panel;
            ui->mouse.drag.type = UI_DRAG_PANEL;
            ui->mouse.drag.panel = panel;

            uiPanelSetFocused(ui, panel);

            UIMouseEvent ev;
            ev.state = &ui->mouse;
            ev.mouse = event;
            rectToLocal(node->rect, x, y, &ev.mouse.x, &ev.mouse.y);

            bool capture = false;
            if (!pre_mouse_event || !pre_mouse_event(panel, ev)) {
                capture = panel->vt->mouseEvent(panel, ev);
            }
            ui->mouse.drag.capture = capture;
        } break;

        case MOUSE1_RELEASED:
            if (ui->mouse.drag.type == UI_DRAG_PANEL) {
                Panel* panel;
                if (ui->mouse.drag.capture) {
                    panel = ui->mouse.drag.panel;
                } else {
                    LayoutNode* node = layoutFindAt(ui->root, x, y);
                    if (!node || node->kind != LAYOUT_LEAF)
                        break;
                    panel = node->panel;
                }

                UIMouseEvent ev;
                ev.state = &ui->mouse;
                ev.mouse = event;
                rectToLocal(panel->layout->rect, x, y, &ev.mouse.x,
                            &ev.mouse.y);

                ui->mouse.drag.type = UI_DRAG_NONE;
                if (!pre_mouse_event || !pre_mouse_event(panel, ev)) {
                    panel->vt->mouseEvent(panel, ev);
                }
            } else {
                ui->mouse.drag.type = UI_DRAG_NONE;
            }
            break;

        case MOUSE1_DRAG: {
            if (ui->mouse.drag.type == UI_DRAG_PANEL) {
                Panel* panel;
                if (ui->mouse.drag.capture) {
                    panel = ui->mouse.drag.panel;
                } else {
                    LayoutNode* node = layoutFindAt(ui->root, x, y);
                    if (!node || node->kind != LAYOUT_LEAF)
                        break;
                    panel = node->panel;
                }

                UIMouseEvent ev;
                ev.state = &ui->mouse;
                ev.mouse = event;
                rectToLocal(panel->layout->rect, x, y, &ev.mouse.x,
                            &ev.mouse.y);

                if (!pre_mouse_event || !pre_mouse_event(panel, ev)) {
                    panel->vt->mouseEvent(panel, ev);
                }
            } else if (ui->mouse.drag.type == UI_DRAG_SEPARATOR) {
                layoutSeparatorDrag(&ui->mouse.drag.separator, x, y);
            }
        } break;

        default: {
            // Send directly to the panel under the mouse
            LayoutNode* node = layoutFindAt(ui->root, x, y);
            if (!node || node->kind != LAYOUT_LEAF)
                break;

            Panel* panel = node->panel;

            UIMouseEvent ev;
            ev.state = &ui->mouse;
            ev.mouse = event;
            rectToLocal(node->rect, x, y, &ev.mouse.x, &ev.mouse.y);

            if (!pre_mouse_event || !pre_mouse_event(panel, ev)) {
                panel->vt->mouseEvent(panel, ev);
            }
        } break;
    }
}

void uiAddPanel(UI* ui,
                Panel* relative_to,
                Panel* new_panel,
                bool leftright,
                bool first) {
    if (!relative_to || !relative_to->layout || !new_panel)
        return;

    if (!new_panel->layout) {
        new_panel->layout = layoutNodeCreateLeaf(new_panel);
    }

    layoutSplit(&ui->root, relative_to->layout, new_panel->layout, leftright,
                first);
}

void uiDetachPanel(UI* ui, Panel* panel) {
    if (!panel || !panel->layout)
        return;
    layoutDetach(&ui->root, panel->layout);
}

void uiClosePanel(UI* ui, Panel* panel) {
    if (!panel || !panel->layout)
        return;

    // Move focused panel
    bool was_focused = (ui->focused_panel == panel);
    if (was_focused) {
        ui->focused_panel->vt->onFocus(ui->focused_panel, false);
        LayoutNode* node = layoutFindNextFocusNode(panel->layout, true);
        if (node && node->kind == LAYOUT_LEAF) {
            ui->focused_panel = node->panel;
        } else {
            ui->focused_panel = NULL;
        }
    }

    // layoutRemove will call panel->vt->destroy
    layoutRemove(&ui->root, panel->layout);

    if (was_focused && ui->focused_panel) {
        ui->focused_panel->vt->onFocus(ui->focused_panel, true);
    }
}

void uiPanelSetEnabled(UI* ui, Panel* panel, bool enabled) {
    if (!panel || !panel->layout)
        return;
    panel->layout->enabled = enabled;
    layoutUpdate(ui->root);
}

void uiPanelSetFocused(UI* ui, Panel* panel) {
    if (ui->focused_panel == panel)
        return;

    if (ui->focused_panel) {
        ui->focused_panel->vt->onFocus(ui->focused_panel, false);
    }

    ui->last_focused_panel = ui->focused_panel;
    ui->focused_panel = panel;

    if (ui->focused_panel) {
        ui->focused_panel->vt->onFocus(ui->focused_panel, true);
    }
}

void uiPanelNavigate(UI* ui, LayoutDirection dir) {
    if (!ui->focused_panel)
        return;

    LayoutNode* next = layoutNavigate(ui->focused_panel->layout, dir);
    if (next && next->kind == LAYOUT_LEAF) {
        uiPanelSetFocused(ui, next->panel);
    }
}

typedef struct UIPanelWalkUserData {
    UIWalkCallback callback;
    void* user_data;
} UIPanelWalkUserData;

static void layoutWalkCallback(LayoutNode* node, void* user_data) {
    if (!node || node->kind != LAYOUT_LEAF)
        return;

    UIPanelWalkUserData* walk_data = (UIPanelWalkUserData*)user_data;
    walk_data->callback(node->panel, walk_data->user_data);
}

void uiPanelWalk(UI* ui, UIWalkCallback callback, void* user_data) {
    if (!ui || !callback)
        return;

    UIPanelWalkUserData walk_data = {
        .callback = callback,
        .user_data = user_data,
    };
    layoutWalk(ui->root, layoutWalkCallback, &walk_data);
}
