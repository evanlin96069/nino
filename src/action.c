#include "action.h"

#include "editor.h"

static int editorPosCmp(int x1, int y1, int x2, int y2) {
    if (y1 < y2)
        return -1;
    if (y1 > y2)
        return 1;
    if (x1 < x2)
        return -1;
    if (x1 > x2)
        return 1;
    return 0;
}

static bool editorPosInsideRange(int x, int y, EditorSelectRange range) {
    return editorPosCmp(x, y, range.start_x, range.start_y) >= 0 &&
           editorPosCmp(x, y, range.end_x, range.end_y) < 0;
}

static void editorPosUpdate(int* x,
                            int* y,
                            EditorSelectRange delete_range,
                            EditorSelectRange insert_range) {
    if (editorPosCmp(*x, *y, delete_range.start_x, delete_range.start_y) < 0)
        return;

    if (editorPosInsideRange(*x, *y, delete_range)) {
        *x = insert_range.end_x;
        *y = insert_range.end_y;
        return;
    }

    if (editorPosCmp(*x, *y, delete_range.end_x, delete_range.end_y) >= 0) {
        int tail_x;
        int tail_y = *y - delete_range.end_y;
        if (tail_y == 0) {
            tail_x = *x - delete_range.end_x;
        } else {
            tail_x = *x;
        }

        if (tail_y == 0) {
            *x = insert_range.end_x + tail_x;
            *y = insert_range.end_y;
        } else {
            *x = tail_x;
            *y = insert_range.end_y + tail_y;
        }
    }
}

void editorApplyEdit(EditorTab* tab, Edit* edit, bool undo) {
    EditorFile* file = editorTabGetFile(tab);

    EditorSelectRange delete_range;
    EditorClipboard* to_add;
    if (undo) {
        delete_range = getClipboardRange(edit->x, edit->y, &edit->after);
        to_add = &edit->before;
    } else {
        delete_range = getClipboardRange(edit->x, edit->y, &edit->before);
        to_add = &edit->after;
    }

    editorDeleteText(file, delete_range);
    editorPasteText(file, to_add, edit->x, edit->y);

    EditorSelectRange insert_range =
        getClipboardRange(edit->x, edit->y, to_add);

    // Update all tabs referencing this file
    for (int i = 0; i < gEditor.split_count; i++) {
        EditorSplit* split = &gEditor.splits[i];
        for (int j = 0; j < split->tab_count; j++) {
            EditorTab* t = &split->tabs[j];
            if (t->file_index != tab->file_index)
                continue;

            if (t == tab) {
                t->cursor.x = insert_range.end_x;
                t->cursor.y = insert_range.end_y;
                t->cursor.is_selected = false;
                editorUpdateSx(t);
                continue;
            }

            editorPosUpdate(&t->cursor.x, &t->cursor.y, delete_range,
                            insert_range);

            if (t->cursor.is_selected) {
                editorPosUpdate(&t->cursor.select_x, &t->cursor.select_y,
                                delete_range, insert_range);
                if (t->cursor.x == t->cursor.select_x &&
                    t->cursor.y == t->cursor.select_y) {
                    t->cursor.is_selected = false;
                }
            }
            editorUpdateSx(t);
        }
    }
}

bool editorUndo(EditorTab* tab) {
    EditorFile* file = editorTabGetFile(tab);

    if (file->action_current == file->action_head)
        return false;

    if (file->read_only && !file->unlocked) {
        // This shouldn't happen since there won't be any edit, but just in case
        return false;
    }

    switch (file->action_current->action->type) {
        case ACTION_EDIT: {
            EditAction* edit = &file->action_current->action->edit;
            editorApplyEdit(tab, &edit->data, true);
            tab->cursor = edit->old_cursor;
        } break;

        case ACTION_ATTRI: {
            AttributeAction* attri = &file->action_current->action->attri;
            file->newline = attri->old_newline;
        } break;
    }

    file->action_current = file->action_current->prev;
    file->dirty--;
    return true;
}

bool editorRedo(EditorTab* tab) {
    EditorFile* file = editorTabGetFile(tab);

    if (!file->action_current->next)
        return false;

    if (file->read_only && !file->unlocked) {
        // This shouldn't happen since there won't be any edit, but just in case
        return false;
    }

    file->action_current = file->action_current->next;

    switch (file->action_current->action->type) {
        case ACTION_EDIT: {
            EditAction* edit = &file->action_current->action->edit;
            editorApplyEdit(tab, &edit->data, false);
            tab->cursor = edit->new_cursor;
        } break;

        case ACTION_ATTRI: {
            AttributeAction* attri = &file->action_current->action->attri;
            file->newline = attri->new_newline;
        } break;
    }

    file->dirty++;
    return true;
}

void editorAppendAction(EditorFile* file, EditorAction* action) {
    if (!action)
        return;

    EditorActionList* node = malloc_s(sizeof(EditorActionList));
    node->action = action;
    node->next = NULL;

    file->dirty++;

    editorFreeActionList(file->action_current->next);

    if (file->action_current == file->action_head) {
        file->action_head->next = node;
        node->prev = file->action_head;
        file->action_current = node;
        return;
    }

    node->prev = file->action_current;
    file->action_current->next = node;
    file->action_current = file->action_current->next;
}

void editorFreeAction(EditorAction* action) {
    if (!action)
        return;

    if (action->type == ACTION_EDIT) {
        editorFreeClipboardContent(&action->edit.data.before);
        editorFreeClipboardContent(&action->edit.data.after);
    }

    free(action);
}

void editorFreeActionList(EditorActionList* thisptr) {
    EditorActionList* temp;
    while (thisptr) {
        temp = thisptr;
        thisptr = thisptr->next;
        editorFreeAction(temp->action);
        free(temp);
    }
}
