#ifndef UI_PANEL_H
#define UI_PANEL_H

#include "terminal.h"

#include "ui/compositor.h"
#include "ui/surface.h"

typedef struct LayoutNode LayoutNode;
typedef struct Panel Panel;

typedef enum PanelKind {
    PANEL_KIND_EDIT,
    PANEL_KIND_EXPLORER,
    PANEL_KIND_PROMPT,
    PANEL_KIND_WELCOME,
} PanelKind;

typedef struct PanelVtable {
    void (*destroy)(Panel* self);
    void (*render)(Panel* self, Surface s);
    bool (*getCursor)(Panel* self, UICursor* out);
    void (*onFocus)(Panel* self, bool focused);
    // EVENT_KEY or EVENT_PASTE
    void (*keyEvent)(Panel* self, KeyEvent event);
    // Return if the panel capture the drag event (UI_MOUSE1_MOVE)
    bool (*mouseEvent)(Panel* self, UIMouseEvent mouse_event);
} PanelVtable;

typedef struct Panel {
    const PanelVtable* vt;
    LayoutNode* layout;
    PanelKind kind;
} Panel;

static inline bool panelIsEnabled(Panel* panel) {
    return panel && panel->layout && panel->layout->enabled;
}

#endif
