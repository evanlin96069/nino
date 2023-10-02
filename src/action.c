#include "action.h"

#include <stdlib.h>

#include "editor.h"
#include "terminal.h"

bool editorUndo(void) {
    if (gCurFile->action_current == gCurFile->action_head)
        return false;

    editorDeleteText(gCurFile->action_current->action->added_range);
    editorPasteText(&gCurFile->action_current->action->deleted_text,
                    gCurFile->action_current->action->deleted_range.start_x,
                    gCurFile->action_current->action->deleted_range.start_y);
    gCurFile->cursor = gCurFile->action_current->action->old_cursor;
    gCurFile->action_current = gCurFile->action_current->prev;
    gCurFile->dirty--;
    return true;
}

bool editorRedo(void) {
    if (!gCurFile->action_current->next)
        return false;

    gCurFile->action_current = gCurFile->action_current->next;
    editorDeleteText(gCurFile->action_current->action->deleted_range);
    editorPasteText(&gCurFile->action_current->action->added_text,
                    gCurFile->action_current->action->added_range.start_x,
                    gCurFile->action_current->action->added_range.start_y);
    gCurFile->cursor = gCurFile->action_current->action->new_cursor;
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
    editorFreeClipboardContent(&action->deleted_text);
    editorFreeClipboardContent(&action->added_text);
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
