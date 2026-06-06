#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "utils.h"

#include "ui/surface.h"

typedef struct Panel Panel;

typedef enum LayoutNodeKind {
    LAYOUT_LEFTRIGHT,
    LAYOUT_TOPBOTTOM,
    LAYOUT_LEAF,
} LayoutNodeKind;

typedef enum LayoutSizeType {
    LAYOUT_SIZE_RATIO,
    LAYOUT_SIZE_FIXED,
} LayoutSizeType;

typedef struct LayoutNode LayoutNode;
struct LayoutNode {
    LayoutNode* parent;
    LayoutNodeKind kind;

    Rect rect;
    LayoutSizeType size_type;
    union {
        float ratio;
        int fixed_size;
    };
    int min_size;
    bool enabled;
    bool resizable;

    union {
        // LAYOUT_LEFTRIGHT/TOPBOTTOM
        VECTOR(LayoutNode*) children;
        // LAYOUT_LEAF
        Panel* panel;
    };
};

typedef struct Separator {
    LayoutNode* parent;
    int index;
    Rect rect;
} Separator;

typedef VECTOR(Separator) VecSeparator;

typedef enum LayoutDirection {
    LAYOUT_DIR_LEFT,
    LAYOUT_DIR_RIGHT,
    LAYOUT_DIR_UP,
    LAYOUT_DIR_DOWN,
} LayoutDirection;

LayoutNode* layoutNodeCreate(LayoutNodeKind kind);
LayoutNode* layoutNodeCreateLeaf(Panel* panel);
LayoutNode* layoutNodeCreateSplit(bool leftright);

void layoutCompute(LayoutNode* node, Rect available, VecSeparator* separators);
void layoutRender(LayoutNode* node, Surface s);
void layoutFree(LayoutNode* node);

void layoutSplit(LayoutNode** root,
                 LayoutNode* node,
                 LayoutNode* new_node,
                 bool leftright);
// Remove and free the node and panel
void layoutRemove(LayoutNode** root, LayoutNode* node);
// Return the node itself if the node is on the edge in the given direction
LayoutNode* layoutNavigate(LayoutNode* node, LayoutDirection dir);

// Return NULL if not found
LayoutNode* layoutFindAt(LayoutNode* node, int x, int y);
Separator* layoutFindSeparatorAt(VecSeparator* separators, int x, int y);

#endif
