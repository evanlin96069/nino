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

static inline MouseEventType inputTypeToMouseEventType(int type) {
    switch (type) {
        case MOUSE_MOVE:
            return UI_MOUSE1_MOVE;
        case MOUSE_PRESSED:
            return UI_MOUSE1_PRESSED;
        case MOUSE_RELEASED:
            return UI_MOUSE1_RELEASED;
        case SCROLL_PRESSED:
            return UI_MOUSE3_PRESSED;
        case SCROLL_RELEASED:
            return UI_MOUSE3_RELEASED;
        case WHEEL_UP:
            return UI_MWHEEL_UP;
        case WHEEL_DOWN:
            return UI_MWHEEL_DOWN;
        default:
            return -1;  // Invalid type
    }
}

void uiProcessInput(UI* ui,
                    EditorInput input,
                    GlobalInputHandler global_input_handler) {
    switch (input.type) {
        case MOUSE_MOVE:
        case MOUSE_PRESSED:
        case MOUSE_RELEASED:
        case SCROLL_PRESSED:
        case SCROLL_RELEASED:
        case WHEEL_UP:
        case WHEEL_DOWN: {
            int x = input.data.cursor.x;
            int y = input.data.cursor.y;
            ui->mouse.type = inputTypeToMouseEventType(input.type);
            ui->mouse.x = x;
            ui->mouse.y = y;

            switch (ui->mouse.type) {
                case UI_MOUSE1_MOVE: {
                    if (ui->mouse.drag.type == DRAG_PANEL) {
                        Panel* panel;
                        if (ui->mouse.drag.capture) {
                            panel = ui->mouse.drag.panel;
                        } else {
                            LayoutNode* node = layoutFindAt(ui->root, x, y);
                            if (!node || node->kind != LAYOUT_LEAF)
                                break;
                            panel = node->panel;
                        }

                        MouseEvent event;
                        event.state = &ui->mouse;
                        rectToLocal(panel->layout->rect, x, y, &event.x,
                                    &event.y);

                        panel->vt->mouseEvent(panel, event);
                    } else if (ui->mouse.drag.type == DRAG_SEPARATOR) {
                        layoutSeparatorDrag(&ui->mouse.drag.separator, x, y);
                    }
                } break;

                case UI_MOUSE1_PRESSED: {
                    if (input.timestamp_ms - ui->mouse.last_click_time <
                        UI_MOUSE_DOUBLE_CLICK_TIME) {
                        ui->mouse.click_count++;
                    } else {
                        ui->mouse.click_count = 1;
                    }
                    ui->mouse.last_click_time = input.timestamp_ms;

                    ui->mouse.drag.start_x = x;
                    ui->mouse.drag.start_y = y;

                    Separator* sep =
                        layoutFindSeparatorAt(&ui->separators, x, y);
                    if (sep) {
                        ui->mouse.drag.type = DRAG_SEPARATOR;
                        ui->mouse.drag.separator = *sep;
                        break;
                    }

                    LayoutNode* node = layoutFindAt(ui->root, x, y);
                    if (!node || node->kind != LAYOUT_LEAF)
                        break;

                    Panel* panel = node->panel;
                    ui->mouse.drag.type = DRAG_PANEL;
                    ui->mouse.drag.panel = panel;

                    ui->focused_panel->vt->onFocus(ui->focused_panel, false);
                    ui->focused_panel = panel;
                    ui->focused_panel->vt->onFocus(ui->focused_panel, true);

                    MouseEvent event;
                    event.state = &ui->mouse;
                    rectToLocal(node->rect, x, y, &event.x, &event.y);

                    ui->mouse.drag.capture =
                        panel->vt->mouseEvent(panel, event);
                } break;

                case UI_MOUSE1_RELEASED:
                    if (ui->mouse.drag.type == DRAG_PANEL) {
                        Panel* panel;
                        if (ui->mouse.drag.capture) {
                            panel = ui->mouse.drag.panel;
                        } else {
                            LayoutNode* node = layoutFindAt(ui->root, x, y);
                            if (!node || node->kind != LAYOUT_LEAF)
                                break;
                            panel = node->panel;
                        }

                        MouseEvent event;
                        event.state = &ui->mouse;
                        rectToLocal(panel->layout->rect, x, y, &event.x,
                                    &event.y);

                        ui->mouse.drag.type = DRAG_NONE;
                        panel->vt->mouseEvent(panel, event);
                    } else {
                        ui->mouse.drag.type = DRAG_NONE;
                    }
                    break;

                case UI_MOUSE3_PRESSED:
                case UI_MOUSE3_RELEASED:
                case UI_MWHEEL_UP:
                case UI_MWHEEL_DOWN: {
                    LayoutNode* node = layoutFindAt(ui->root, x, y);
                    if (!node || node->kind != LAYOUT_LEAF)
                        break;

                    MouseEvent event;
                    event.state = &ui->mouse;
                    rectToLocal(node->rect, x, y, &event.x, &event.y);

                    node->panel->vt->mouseEvent(node->panel, event);
                } break;

                default:
                    break;
            }
        } break;

        default:
            if (global_input_handler(input))
                break;
            if (ui->focused_panel) {
                ui->focused_panel->vt->keyEvent(ui->focused_panel, input);
            }
            break;
    }
}

void uiClosePanel(UI* ui, Panel* panel) {
    if (!panel || !panel->layout)
        return;

    // Move focused panel
    if (ui->focused_panel == panel) {
        ui->focused_panel->vt->onFocus(ui->focused_panel, false);
        LayoutNode* node = layoutFindNextFocusNode(panel->layout, true);
        if (node && node->kind == LAYOUT_LEAF) {
            ui->focused_panel = node->panel;
        } else {
            ui->focused_panel = NULL;
        }
    }

    layoutRemove(&ui->root, panel->layout);

    if (ui->focused_panel) {
        ui->focused_panel->vt->onFocus(ui->focused_panel, true);
    }
}
