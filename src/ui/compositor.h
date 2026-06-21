#ifndef UI_COMPOSITOR_H
#define UI_COMPOSITOR_H

#include "terminal.h"

#include "ui/layout.h"

#define UI_MOUSE_DOUBLE_CLICK_TIME 500  // ms

typedef enum MouseEventType {
    UI_MOUSE1_MOVE,
    UI_MOUSE1_PRESSED,
    UI_MOUSE1_RELEASED,
    UI_MOUSE2_PRESSED,
    UI_MOUSE2_RELEASED,
    UI_MOUSE3_PRESSED,
    UI_MOUSE3_RELEASED,
    UI_MWHEEL_UP,
    UI_MWHEEL_DOWN,
} MouseEventType;

typedef enum DragType {
    DRAG_NONE = 0,
    DRAG_PANEL,
    DRAG_SEPARATOR,
} DragType;

typedef struct DragState {
    DragType type;
    bool capture;  // panel capture
    union {
        Separator separator;
        Panel* panel;
    };
    int start_x;
    int start_y;
} DragState;

typedef struct MouseState {
    MouseEventType type;
    int x;
    int y;

    // These are for mouse1
    int64_t last_click_time;
    int click_count;

    DragState drag;
} MouseState;

typedef struct MouseEvent {
    const MouseState* state;
    int x, y;  // local coordinates
} MouseEvent;

typedef struct UI {
    LayoutNode* root;
    VecSeparator separators;
    Panel* focused_panel;
    MouseState mouse;
} UI;

typedef bool (*GlobalInputHandler)(EditorInput input);

static inline void uiInit(UI* ui) {
    memset(ui, 0, sizeof(UI));
}

void uiFree(UI* ui);

void uiComposite(UI* ui, Surface s, ScreenStyle sep_style);
void uiProcessInput(UI* ui,
                    EditorInput input,
                    GlobalInputHandler global_input_handler);
void uiClosePanel(UI* ui, Panel* panel);

#endif
