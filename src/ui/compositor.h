#ifndef UI_COMPOSITOR_H
#define UI_COMPOSITOR_H

#include "terminal.h"

#include "ui/layout.h"

#define UI_MOUSE_DOUBLE_CLICK_TIME 500  // ms

typedef enum UIMouseEventType {
    UI_MOUSE1_MOVE,
    UI_MOUSE1_PRESSED,
    UI_MOUSE1_RELEASED,
    UI_MOUSE2_PRESSED,
    UI_MOUSE2_RELEASED,
    UI_MOUSE3_PRESSED,
    UI_MOUSE3_RELEASED,
    UI_MWHEEL_UP,
    UI_MWHEEL_DOWN,
} UIMouseEventType;

typedef enum UIDragType {
    UI_DRAG_NONE = 0,
    UI_DRAG_PANEL,
    UI_DRAG_SEPARATOR,
} UIDragType;

typedef struct UIDragState {
    UIDragType type;
    bool capture;  // panel capture
    union {
        Separator separator;
        Panel* panel;
    };
    int start_x;
    int start_y;
} UIDragState;

typedef struct UIMouseState {
    UIMouseEventType type;
    int x;
    int y;

    // These are for mouse1
    int64_t last_click_time;
    int click_count;

    UIDragState drag;
} UIMouseState;

typedef struct UIMouseEvent {
    const UIMouseState* state;
    int x, y;  // local coordinates
} UIMouseEvent;

typedef struct UICursor {
    bool visible;
    int x;
    int y;
} UICursor;

typedef struct UI {
    LayoutNode* root;
    VecSeparator separators;
    Panel* focused_panel;
    UIMouseState mouse;
} UI;

typedef bool (*UIGlobalInputHandler)(EditorInput input);

static inline void uiInit(UI* ui) {
    memset(ui, 0, sizeof(UI));
}

void uiFree(UI* ui);

void uiComposite(UI* ui, Surface s, ScreenStyle sep_style);
bool uiGetCursor(UI* ui, UICursor* out);
void uiProcessInput(UI* ui,
                    EditorInput input,
                    UIGlobalInputHandler global_input_handler);
void uiClosePanel(UI* ui, Panel* panel);

#endif
