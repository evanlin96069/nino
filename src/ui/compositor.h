#ifndef UI_COMPOSITOR_H
#define UI_COMPOSITOR_H

#include "terminal.h"

#include "ui/layout.h"

#define UI_MOUSE_DOUBLE_CLICK_TIME 500  // ms

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
    // These are for mouse1
    int64_t last_click_time;
    int click_count;

    UIDragState drag;
} UIMouseState;

typedef struct UIMouseEvent {
    const UIMouseState* state;
    MouseEvent mouse;
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
    Panel* last_focused_panel;
    UIMouseState mouse;
} UI;

static inline void uiInit(UI* ui) {
    memset(ui, 0, sizeof(UI));
}

void uiFree(UI* ui);

void uiComposite(UI* ui, Surface s, ScreenStyle sep_style);
bool uiGetCursor(UI* ui, UICursor* out);

// Input
typedef bool (*UIPreKeyEvent)(Panel* panel, KeyEvent event);
void uiProcessKeyEvent(UI* ui, KeyEvent event, UIPreKeyEvent pre_key_event);
typedef bool (*UIPreMouseEvent)(Panel* panel, UIMouseEvent event);
void uiProcessMouseEvent(UI* ui,
                         MouseEvent event,
                         uint64_t timestamp_ms,
                         UIPreMouseEvent pre_mouse_event);

void uiAddPanel(UI* ui,
                Panel* relative_to,
                Panel* new_panel,
                bool leftright,
                bool first);
void uiDetachPanel(UI* ui, Panel* panel);
void uiClosePanel(UI* ui, Panel* panel);

void uiPanelSetEnabled(UI* ui, Panel* panel, bool enabled);
void uiPanelSetFocused(UI* ui, Panel* panel);
void uiPanelNavigate(UI* ui, LayoutDirection dir);

typedef void (*UIWalkCallback)(Panel* panel, void* user_data);
void uiPanelWalk(UI* ui, UIWalkCallback callback, void* user_data);

#endif
