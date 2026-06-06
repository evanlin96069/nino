#include "ui/layout.h"

#include "ui/panel.h"

LayoutNode* layoutNodeCreate(LayoutNodeKind kind) {
    LayoutNode* result = calloc_s(1, sizeof(LayoutNode));
    result->kind = kind;
    result->size_type = LAYOUT_SIZE_RATIO;
    result->ratio = 1.0f;
    result->min_size = 1;
    result->enabled = true;
    result->resizable = true;
    return result;
}

LayoutNode* layoutNodeCreateLeaf(Panel* panel) {
    LayoutNode* result = layoutNodeCreate(LAYOUT_LEAF);
    result->panel = panel;
    return result;
}

LayoutNode* layoutNodeCreateSplit(bool leftright) {
    LayoutNode* result =
        layoutNodeCreate(leftright ? LAYOUT_LEFTRIGHT : LAYOUT_TOPBOTTOM);
    return result;
}

static bool layoutIsEnabled(const LayoutNode* node) {
    if (!node)
        return false;
    if (!node->enabled)
        return false;
    if (node->kind == LAYOUT_LEAF)
        return true;
    for (uint32_t i = 0; i < node->children.size; i++) {
        const LayoutNode* child = node->children.data[i];
        if (layoutIsEnabled(child)) {
            return true;
        }
    }
    return false;
}

void layoutCompute(LayoutNode* node, Rect available, VecSeparator* separators) {
    if (!node)
        return;
    if (!layoutIsEnabled(node))
        return;

    node->rect = available;

    if (node->kind == LAYOUT_LEAF)
        return;

    int last;
    for (last = node->children.size - 1; last >= 0; last--) {
        const LayoutNode* child = node->children.data[last];
        if (layoutIsEnabled(child))
            break;
    }

    if (last < 0)
        return;

    bool leftright = (node->kind == LAYOUT_LEFTRIGHT);
    int total_size = leftright ? available.w : available.h;

    int content_size = total_size;
    float total_ratio = 0.0f;
    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* child = node->children.data[i];
        // TODO: cache the layoutIsEnabled(child) results.
        // Don't want to allocate memory for the cache.
        // Need a stack fallback allocator backed vector.
        if (!layoutIsEnabled(child))
            continue;

        if (child->size_type == LAYOUT_SIZE_FIXED) {
            content_size -= child->fixed_size;
        } else {
            total_ratio += child->ratio;
        }

        // Separator
        if (child->resizable && (int)i != last) {
            content_size--;
        }
    }

    if (total_ratio <= 0.0f)
        total_ratio = 1.0f;
    if (content_size < 0)
        content_size = 0;

    int offset = 0;
    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* child = node->children.data[i];
        if (!layoutIsEnabled(child))
            continue;

        int size = 0;
        if (offset < total_size) {
            if (child->size_type == LAYOUT_SIZE_FIXED) {
                size = child->fixed_size;
            } else {
                size = (int)((child->ratio / total_ratio) * content_size);
            }

            if (size < child->min_size)
                size = child->min_size;
            if (offset + size > total_size)
                size = total_size - offset;
        }

        Rect rect;
        if (leftright) {
            rect.x = available.x + offset;
            rect.y = available.y;
            rect.w = size;
            rect.h = available.h;
        } else {
            rect.x = available.x;
            rect.y = available.y + offset;
            rect.w = available.w;
            rect.h = size;
        }
        layoutCompute(child, rect, separators);
        offset += size;

        // Separator
        if (offset < total_size && child->resizable && (int)i != last) {
            Separator sep;
            sep.parent = node;
            sep.index = i;
            if (leftright) {
                sep.rect.x = available.x + offset;
                sep.rect.y = available.y;
                sep.rect.w = 1;
                sep.rect.h = available.h;
            } else {
                sep.rect.x = available.x;
                sep.rect.y = available.y + offset;
                sep.rect.w = available.w;
                sep.rect.h = 1;
            }

            vector_push(*separators, sep);
            offset++;
        }
    }
}

void layoutRender(LayoutNode* node, Surface s) {
    if (!node)
        return;
    if (!layoutIsEnabled(node))
        return;

    if (node->rect.w == 0 || node->rect.h == 0)
        return;

    if (node->kind == LAYOUT_LEAF) {
        Panel* p = node->panel;
        p->vt->render(p, surfaceSub(s, node->rect));
        return;
    }

    for (uint32_t i = 0; i < node->children.size; i++) {
        layoutRender(node->children.data[i], s);
    }
}

void layoutFree(LayoutNode* node) {
    if (!node)
        return;
    if (node->kind == LAYOUT_LEAF) {
        Panel* p = node->panel;
        p->vt->destroy(p);
    } else {
        for (uint32_t i = 0; i < node->children.size; i++) {
            layoutFree(node->children.data[i]);
        }
        vector_free(node->children);
    }
    free(node);
}

void layoutSplit(LayoutNode** root,
                 LayoutNode* node,
                 LayoutNode* new_node,
                 bool leftright) {
    if (!node || !new_node || !root)
        return;

    LayoutNode* parent = node->parent;
    if (parent &&
        (parent->kind != (leftright ? LAYOUT_LEFTRIGHT : LAYOUT_TOPBOTTOM))) {
        new_node->parent = parent;
        for (uint32_t i = 0; i < parent->children.size; i++) {
            if (parent->children.data[i] == node) {
                vector_insert(parent->children, i + 1, new_node);
                break;
            }
        }
        return;
    }

    // Create a new split node
    LayoutNode* split_node = layoutNodeCreateSplit(leftright);
    split_node->parent = parent;
    split_node->size_type = node->size_type;
    if (node->size_type == LAYOUT_SIZE_RATIO) {
        split_node->ratio = node->ratio;
    } else {
        split_node->fixed_size = node->fixed_size;
    }
    split_node->min_size = node->min_size;
    split_node->enabled = node->enabled;
    split_node->resizable = node->resizable;

    vector_push(split_node->children, node);
    vector_push(split_node->children, new_node);
    new_node->parent = split_node;

    if (parent) {
        for (uint32_t i = 0; i < parent->children.size; i++) {
            if (parent->children.data[i] == node) {
                parent->children.data[i] = split_node;
                break;
            }
        }
    } else {
        *root = split_node;
    }
}

void layoutRemove(LayoutNode** root, LayoutNode* node) {
    if (!node || !root)
        return;
    LayoutNode* parent = node->parent;
    if (!parent) {
        *root = NULL;
        layoutFree(node);
        return;
    }

    for (uint32_t i = 0; i < parent->children.size; i++) {
        if (parent->children.data[i] == node) {
            vector_erase(parent->children, i);
            layoutFree(node);
            break;
        }
    }

    // If parent has only one child left, promote the child
    if (parent->children.size == 1) {
        LayoutNode* child = parent->children.data[0];
        child->parent = parent->parent;
        if (!parent->parent) {
            *root = child;
        } else {
            for (uint32_t i = 0; i < parent->parent->children.size; i++) {
                if (parent->parent->children.data[i] == parent) {
                    parent->parent->children.data[i] = child;
                    break;
                }
            }
        }
        parent->children.size = 0;  // Don't free the child
        layoutFree(parent);
    }
}

static LayoutNode* layoutFindNextSibling(LayoutNode* node) {
    if (!node || !node->parent)
        return NULL;
    LayoutNode* parent = node->parent;
    bool found_self = false;
    for (uint32_t i = 0; i < parent->children.size; i++) {
        LayoutNode* child = parent->children.data[i];
        if (!layoutIsEnabled(child))
            continue;
        if (child == node) {
            found_self = true;
        } else if (found_self) {
            return child;
        }
    }
    return NULL;
}

static LayoutNode* layoutFindPrevSibling(LayoutNode* node) {
    if (!node || !node->parent)
        return NULL;
    LayoutNode* parent = node->parent;
    LayoutNode* prev = NULL;
    for (uint32_t i = 0; i < parent->children.size; i++) {
        LayoutNode* child = parent->children.data[i];
        if (!layoutIsEnabled(child))
            continue;
        if (child == node) {
            return prev;
        }
        prev = child;
    }
    return NULL;
}

static LayoutNode* layoutFindFirstEnabledChild(LayoutNode* node) {
    if (!node)
        return NULL;
    if (node->kind == LAYOUT_LEAF)
        return NULL;
    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* child = node->children.data[i];
        if (!layoutIsEnabled(child))
            continue;
        return child;
    }
    return NULL;
}

static LayoutNode* layoutFindLastEnabledChild(LayoutNode* node) {
    if (!node)
        return NULL;
    if (node->kind == LAYOUT_LEAF)
        return NULL;
    for (int i = node->children.size - 1; i >= 0; i--) {
        LayoutNode* child = node->children.data[i];
        if (!layoutIsEnabled(child))
            continue;
        return child;
    }
    return NULL;
}

LayoutNode* layoutNavigate(LayoutNode* node, LayoutDirection dir) {
    if (!node || !layoutIsEnabled(node))
        return NULL;
    bool positive = (dir == LAYOUT_DIR_RIGHT || dir == LAYOUT_DIR_DOWN);
    LayoutNodeKind expected_parent_kind =
        (dir == LAYOUT_DIR_LEFT || dir == LAYOUT_DIR_RIGHT) ? LAYOUT_LEFTRIGHT
                                                            : LAYOUT_TOPBOTTOM;
    // Ascend
    LayoutNode* current = node;
    while (current) {
        if (current->parent && current->parent->kind == expected_parent_kind) {
            LayoutNode* sibling = positive ? layoutFindNextSibling(current)
                                           : layoutFindPrevSibling(current);
            if (sibling) {
                current = sibling;
                break;
            }
        }
        current = current->parent;
    }

    // Descend
    while (current && current->kind != LAYOUT_LEAF) {
        current = positive ? layoutFindFirstEnabledChild(current)
                           : layoutFindLastEnabledChild(current);
    }

    if (!current)
        return node;
    return current;
}

LayoutNode* layoutFindAt(LayoutNode* node, int x, int y) {
    if (!node)
        return NULL;
    if (!layoutIsEnabled(node))
        return NULL;
    if (!rectContains(node->rect, x, y))
        return NULL;

    if (node->kind == LAYOUT_LEAF)
        return node;

    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* found = layoutFindAt(node->children.data[i], x, y);
        if (found)
            return found;
    }
    return NULL;
}

Separator* layoutFindSeparatorAt(VecSeparator* separators, int x, int y) {
    for (uint32_t i = 0; i < separators->size; i++) {
        Separator* sep = &separators->data[i];
        if (rectContains(sep->rect, x, y)) {
            return sep;
        }
    }
    return NULL;
}
