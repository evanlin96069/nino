#ifndef UI_PANEL_H
#define UI_PANEL_H

#include "terminal.h"

#include "ui/compositor.h"
#include "ui/surface.h"

typedef struct LayoutNode LayoutNode;
typedef struct Panel Panel;

typedef struct PanelVtable {
    void (*destroy)(Panel* self);
    void (*render)(Panel* self, Surface s);
    bool (*getCursor)(Panel* self, UICursor* out);
    void (*onFocus)(Panel* self, bool focused);
    void (*keyEvent)(Panel* self, EditorInput input);
    // Return if the panel capture the drag event (UI_MOUSE1_MOVE)
    bool (*mouseEvent)(Panel* self, UIMouseEvent mouse_event);
} PanelVtable;

typedef struct Panel {
    const PanelVtable* vt;
    LayoutNode* layout;
} Panel;

#endif
