#ifndef UI_PANEL_H
#define UI_PANEL_H

#include "terminal.h"

#include "ui/surface.h"

typedef struct LayoutNode LayoutNode;
typedef struct Panel Panel;

typedef struct PanelVtable {
    void (*destroy)(Panel* self);
    void (*render)(Panel* self, Surface s);
    void (*onFocus)(Panel* self);
    bool (*keyEvent)(Panel* self, EditorInput input);
    bool (*mouseEvent)(Panel* self,
                       EditorInput input,
                       int local_x,
                       int local_y);
} PanelVtable;

typedef struct Panel {
    const PanelVtable* vt;
    LayoutNode* layout;
} Panel;

#endif
