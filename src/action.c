#include "action.h"

#include "editor.h"

bool editorUndo(void) {
    if (gCurFile->action_current == gCurFile->action_head)
        return false;

    switch (gCurFile->action_current->action->type) {
        case ACTION_EDIT: {
            EditAction* edit = &gCurFile->action_current->action->edit;
            editorDeleteText(edit->added_range);
            editorPasteText(&edit->deleted_text, edit->deleted_range.start_x,
                            edit->deleted_range.start_y);
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
            editorDeleteText(edit->deleted_range);
            editorPasteText(&edit->added_text, edit->added_range.start_x,
                            edit->added_range.start_y);
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
        editorFreeClipboardContent(&action->edit.deleted_text);
        editorFreeClipboardContent(&action->edit.added_text);
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
