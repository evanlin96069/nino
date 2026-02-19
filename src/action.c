#include "action.h"

#include "editor.h"

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
    for (int i = 0; i < gEditor.split_count; ++i) {
        EditorSplit* split = &gEditor.splits[i];
        for (int j = 0; j < split->tab_count; ++j) {
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

            // Cursor is after the edit
            if (t->cursor.y > delete_range.end_y ||
                (t->cursor.y == delete_range.end_y &&
                 t->cursor.x >= delete_range.end_x)) {
                int dx = insert_range.end_x - delete_range.end_x;
                int dy = insert_range.end_y - delete_range.end_y;
                t->cursor.x += dx;
                t->cursor.y += dy;
            } else if (t->cursor.y > delete_range.start_y ||
                       (t->cursor.y == delete_range.start_y &&
                        t->cursor.x >= delete_range.start_x)) {
                // Cursor is inside the edited region
                t->cursor.x = insert_range.end_x;
                t->cursor.y = insert_range.end_y;
            }

            if (t->cursor.is_selected) {
                if (!((t->cursor.y < delete_range.start_y) ||
                      (t->cursor.select_y > delete_range.end_y))) {
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
