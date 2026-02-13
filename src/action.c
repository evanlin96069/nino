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
    tab->cursor.x = insert_range.end_x;
    tab->cursor.y = insert_range.end_y;
    editorUpdateSx(tab);
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
