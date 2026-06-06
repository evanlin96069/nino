#ifndef UI_COMPOSITOR_H
#define UI_COMPOSITOR_H

#include "terminal.h"

#include "ui/layout.h"

typedef struct UI {
    LayoutNode* root;
    VecSeparator separators;
    Panel* focused_panel;
} UI;

static inline void uiInit(UI* ui) {
    memset(ui, 0, sizeof(UI));
}
void uiFree(UI* ui);

void uiComposite(UI* ui, Surface s, ScreenStyle sep_style);
void uiProcessInput(UI* ui, EditorInput input);

#endif
