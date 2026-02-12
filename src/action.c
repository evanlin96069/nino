#include "action.h"

#include "editor.h"

void editorApplyEdit(Edit* edit, bool undo) {
    EditorSelectRange delete_range;
    EditorClipboard* to_add;
    if (undo) {
        delete_range = getClipboardRange(edit->x, edit->y, &edit->after);
        to_add = &edit->before;
    } else {
        delete_range = getClipboardRange(edit->x, edit->y, &edit->before);
        to_add = &edit->after;
    }

    editorDeleteText(delete_range);
    editorPasteText(to_add, edit->x, edit->y);
}

bool editorUndo(void) {
    if (gCurFile->action_current == gCurFile->action_head)
        return false;

    switch (gCurFile->action_current->action->type) {
        case ACTION_EDIT: {
            EditAction* edit = &gCurFile->action_current->action->edit;
            editorApplyEdit(&edit->data, true);
            gCurFile->cursor = edit->old_cursor;
        } break;

        case ACTION_ATTRI: {
            AttributeAction* attri = &gCurFile->action_current->action->attri;
            gCurFile->newline = attri->old_newline;
        } break;
    }

    gCurFile->action_current = gCurFile->action_current->prev;
    gCurFile->dirty--;
    return true;
}

bool editorRedo(void) {
    if (!gCurFile->action_current->next)
        return false;

    gCurFile->action_current = gCurFile->action_current->next;

    switch (gCurFile->action_current->action->type) {
        case ACTION_EDIT: {
            EditAction* edit = &gCurFile->action_current->action->edit;
            editorApplyEdit(&edit->data, false);
            gCurFile->cursor = edit->new_cursor;
        } break;

        case ACTION_ATTRI: {
            AttributeAction* attri = &gCurFile->action_current->action->attri;
            gCurFile->newline = attri->new_newline;
        } break;
    }

    gCurFile->dirty++;
    return true;
}

void editorAppendAction(EditorAction* action) {
    if (!action)
        return;

    EditorActionList* node = malloc_s(sizeof(EditorActionList));
    node->action = action;
    node->next = NULL;

    gCurFile->dirty++;

    editorFreeActionList(gCurFile->action_current->next);

    if (gCurFile->action_current == gCurFile->action_head) {
        gCurFile->action_head->next = node;
        node->prev = gCurFile->action_head;
        gCurFile->action_current = node;
        return;
    }

    node->prev = gCurFile->action_current;
    gCurFile->action_current->next = node;
    gCurFile->action_current = gCurFile->action_current->next;
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
