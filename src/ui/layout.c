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
    result->has_enabled_content = true;
    return result;
}

LayoutNode* layoutNodeCreateLeaf(Panel* panel) {
    LayoutNode* result = layoutNodeCreate(LAYOUT_LEAF);
    result->panel = panel;
    panel->layout = result;
    return result;
}

LayoutNode* layoutNodeCreateSplit(bool leftright) {
    LayoutNode* result =
        layoutNodeCreate(leftright ? LAYOUT_LEFTRIGHT : LAYOUT_TOPBOTTOM);
    return result;
}

bool layoutAppendChild(LayoutNode* parent, LayoutNode* child) {
    if (!parent || !child || parent->kind == LAYOUT_LEAF)
        return false;

    child->parent = parent;
    vector_push(parent->children, child);
    return true;
}

bool layoutInsertChild(LayoutNode* parent, uint32_t index, LayoutNode* child) {
    if (!parent || !child || parent->kind == LAYOUT_LEAF)
        return false;
    if (index > parent->children.size)
        return false;

    child->parent = parent;
    vector_insert(parent->children, index, child);
    return true;
}

static void layoutComputeEnabledCache(LayoutNode* node) {
    if (!node)
        return;

    if (node->kind == LAYOUT_LEAF) {
        node->has_enabled_content = node->enabled;
    } else {
        bool child_has_enabled_content = false;
        for (uint32_t i = 0; i < node->children.size; i++) {
            layoutComputeEnabledCache(node->children.data[i]);
            child_has_enabled_content |=
                node->children.data[i]->has_enabled_content;
        }
        node->has_enabled_content = node->enabled && child_has_enabled_content;
    }
}

void layoutUpdate(LayoutNode* root) {
    layoutComputeEnabledCache(root);
    layoutCompute(root, root->rect, NULL);
}

static int layoutFindFirstEnabledChildIndex(LayoutNode* node) {
    if (!node)
        return -1;
    if (node->kind == LAYOUT_LEAF)
        return -1;
    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* child = node->children.data[i];
        if (!child->has_enabled_content)
            continue;
        return (int)i;
    }
    return -1;
}

static int layoutFindLastEnabledChildIndex(LayoutNode* node) {
    if (!node)
        return -1;
    if (node->kind == LAYOUT_LEAF)
        return -1;
    for (int i = node->children.size - 1; i >= 0; i--) {
        LayoutNode* child = node->children.data[i];
        if (!child->has_enabled_content)
            continue;
        return (int)i;
    }
    return -1;
}

static inline LayoutNode* layoutFindFirstEnabledChild(LayoutNode* node) {
    int index = layoutFindFirstEnabledChildIndex(node);
    if (index < 0)
        return NULL;
    return node->children.data[index];
}

static inline LayoutNode* layoutFindLastEnabledChild(LayoutNode* node) {
    int index = layoutFindLastEnabledChildIndex(node);
    if (index < 0)
        return NULL;
    return node->children.data[index];
}

static bool layoutCanPlaceSeparator(LayoutNode* node, int index, int last) {
    if (!node || node->kind == LAYOUT_LEAF)
        return false;
    if (index < 0 || index == last)
        return false;
    if ((uint32_t)index >= node->children.size)
        return false;

    LayoutNode* current = node->children.data[index];
    if (!current->has_enabled_content || !current->resizable)
        return false;

    for (uint32_t i = (uint32_t)index + 1; i < node->children.size; i++) {
        LayoutNode* next = node->children.data[i];
        if (!next->has_enabled_content)
            continue;
        return next->resizable;
    }

    return false;
}

static bool layoutComputeSizing(LayoutNode* node,
                                int last,
                                int total_size,
                                int* content_size,
                                float* total_ratio) {
    if (!node || node->kind == LAYOUT_LEAF)
        return false;

    if (last < 0)
        return false;

    int size = total_size;
    float ratio = 0.0f;
    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* child = node->children.data[i];
        if (!child->has_enabled_content)
            continue;

        if (child->size_type == LAYOUT_SIZE_FIXED) {
            size -= child->fixed_size;
        } else {
            ratio += child->ratio;
        }

        // Separator
        if (layoutCanPlaceSeparator(node, (int)i, last)) {
            size--;
        }
    }

    if (ratio <= 0.0f)
        ratio = 1.0f;
    if (size < 0)
        size = 0;

    if (content_size)
        *content_size = size;
    if (total_ratio)
        *total_ratio = ratio;
    return true;
}

void layoutCompute(LayoutNode* node, Rect available, VecSeparator* separators) {
    if (!node || !node->has_enabled_content)
        return;

    node->rect = available;

    if (node->kind == LAYOUT_LEAF)
        return;

    int last = layoutFindLastEnabledChildIndex(node);
    if (last < 0)
        return;

    bool leftright = (node->kind == LAYOUT_LEFTRIGHT);
    int total_size = leftright ? available.w : available.h;

    int content_size;
    float total_ratio;
    if (!layoutComputeSizing(node, last, total_size, &content_size,
                             &total_ratio))
        return;

    int offset = 0;
    for (uint32_t i = 0; i < node->children.size; i++) {
        LayoutNode* child = node->children.data[i];
        if (!child->has_enabled_content)
            continue;

        int size = 0;
        if (offset < total_size) {
            if (child->size_type == LAYOUT_SIZE_FIXED) {
                size = child->fixed_size;
            } else if ((int)i == last) {
                size = total_size - offset;
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
        if (offset < total_size &&
            layoutCanPlaceSeparator(node, (int)i, last)) {
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

            if (separators)
                vector_push(*separators, sep);
            offset++;
        }
    }
}

void layoutRender(LayoutNode* node, Surface s) {
    if (!node || !node->has_enabled_content)
        return;

    if (node->rect.w == 0 || node->rect.h == 0)
        return;

    if (node->kind == LAYOUT_LEAF) {
        Panel* p = node->panel;
        p->vt->render(p, surfaceSub(s, node->rect));
        return;
    } else {
        for (uint32_t i = 0; i < node->children.size; i++) {
            layoutRender(node->children.data[i], s);
        }
    }
}

void layoutFree(LayoutNode* node) {
    if (!node)
        return;
    if (node->kind == LAYOUT_LEAF) {
        Panel* p = node->panel;
        if (p) {
            p->vt->destroy(p);
            free(p);
        }
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
        (parent->kind == (leftright ? LAYOUT_LEFTRIGHT : LAYOUT_TOPBOTTOM))) {
        for (uint32_t i = 0; i < parent->children.size; i++) {
            if (parent->children.data[i] == node) {
                layoutInsertChild(parent, i + 1, new_node);
                break;
            }
        }
        layoutUpdate(*root);
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

    layoutAppendChild(split_node, node);
    layoutAppendChild(split_node, new_node);

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

    layoutUpdate(*root);
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

    layoutUpdate(*root);
}

static LayoutNode* layoutFindNextEnabledSibling(LayoutNode* node) {
    if (!node || !node->parent)
        return NULL;
    LayoutNode* parent = node->parent;
    bool found_self = false;
    for (uint32_t i = 0; i < parent->children.size; i++) {
        LayoutNode* child = parent->children.data[i];
        if (!child->has_enabled_content)
            continue;
        if (child == node) {
            found_self = true;
        } else if (found_self) {
            return child;
        }
    }
    return NULL;
}

static LayoutNode* layoutFindPrevEnabledSibling(LayoutNode* node) {
    if (!node || !node->parent)
        return NULL;
    LayoutNode* parent = node->parent;
    LayoutNode* prev = NULL;
    for (uint32_t i = 0; i < parent->children.size; i++) {
        LayoutNode* child = parent->children.data[i];
        if (!child->has_enabled_content)
            continue;
        if (child == node) {
            return prev;
        }
        prev = child;
    }
    return NULL;
}

LayoutNode* layoutNavigate(LayoutNode* node, LayoutDirection dir) {
    if (!node || !node->has_enabled_content)
        return NULL;
    bool positive = (dir == LAYOUT_DIR_RIGHT || dir == LAYOUT_DIR_DOWN);
    LayoutNodeKind expected_parent_kind =
        (dir == LAYOUT_DIR_LEFT || dir == LAYOUT_DIR_RIGHT) ? LAYOUT_LEFTRIGHT
                                                            : LAYOUT_TOPBOTTOM;
    // Ascend
    LayoutNode* current = node;
    while (current) {
        if (current->parent && current->parent->kind == expected_parent_kind) {
            LayoutNode* sibling = positive
                                      ? layoutFindNextEnabledSibling(current)
                                      : layoutFindPrevEnabledSibling(current);
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

    return current;
}

LayoutNode* layoutFindNextFocusNode(LayoutNode* node, bool prefer_next) {
    if (!node || !node->has_enabled_content)
        return NULL;

    // Ascend
    LayoutNode* current = node;
    bool descend_first = !prefer_next;
    while (current) {
        LayoutNode* sibling = prefer_next
                                  ? layoutFindNextEnabledSibling(current)
                                  : layoutFindPrevEnabledSibling(current);
        if (sibling) {
            descend_first = !prefer_next;
            current = sibling;
            break;
        }
        sibling = prefer_next ? layoutFindPrevEnabledSibling(current)
                              : layoutFindNextEnabledSibling(current);
        if (sibling) {
            descend_first = prefer_next;
            current = sibling;
            break;
        }
        current = current->parent;
    }

    // Descend
    while (current && current->kind != LAYOUT_LEAF) {
        current = descend_first ? layoutFindFirstEnabledChild(current)
                                : layoutFindLastEnabledChild(current);
    }

    return current;
}

void layoutSeparatorDrag(Separator* sep, int x, int y) {
    if (!sep || !sep->parent)
        return;

    LayoutNode* parent = sep->parent;
    if (parent->kind != LAYOUT_LEFTRIGHT && parent->kind != LAYOUT_TOPBOTTOM)
        return;
    if (!parent->has_enabled_content)
        return;

    bool leftright = parent->kind == LAYOUT_LEFTRIGHT;

    if (sep->index < 0 || (uint32_t)sep->index >= parent->children.size)
        return;

    int last = layoutFindLastEnabledChildIndex(parent);
    if (last == -1 || last == sep->index)
        return;

    LayoutNode* node = parent->children.data[sep->index];
    if (!node->has_enabled_content)
        return;

    LayoutNode* next = layoutFindNextEnabledSibling(node);
    if (!next || !next->has_enabled_content)
        return;

    int curr_pos = (leftright ? node->rect.x : node->rect.y);
    int lower_bound = curr_pos + node->min_size;
    int upper_bound = (leftright ? next->rect.x + next->rect.w
                                 : next->rect.y + next->rect.h) -
                      next->min_size - 1;
    int desired_pos = (leftright ? x : y);
    if (desired_pos < lower_bound)
        desired_pos = lower_bound;
    if (desired_pos > upper_bound)
        desired_pos = upper_bound;

    int desired_size = desired_pos - curr_pos;

    if (node->size_type == LAYOUT_SIZE_FIXED) {
        node->fixed_size = desired_size;
        return;
    }

    // Ratio
    if (next->size_type == LAYOUT_SIZE_FIXED)
        return;

    int next_end =
        (leftright ? next->rect.x + next->rect.w : next->rect.y + next->rect.h);
    int new_size_next = next_end - desired_pos - 1;
    if (desired_size < 0 || new_size_next < 0)
        return;

    float total_new = (float)(desired_size + new_size_next);
    if (total_new <= 0)
        return;

    float combined_ratio = node->ratio + next->ratio;
    node->ratio = combined_ratio * (float)desired_size / total_new;
    next->ratio = combined_ratio * (float)new_size_next / total_new;
}

LayoutNode* layoutFindAt(LayoutNode* node, int x, int y) {
    if (!node)
        return NULL;
    if (!node->has_enabled_content)
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

void layoutWalk(LayoutNode* node,
                LayoutWalkCallback callback,
                void* user_data) {
    if (!node || !callback)
        return;

    if (!node->has_enabled_content)
        return;

    callback(node, user_data);

    if (node->kind != LAYOUT_LEAF) {
        for (uint32_t i = 0; i < node->children.size; i++) {
            layoutWalk(node->children.data[i], callback, user_data);
        }
    }
}
