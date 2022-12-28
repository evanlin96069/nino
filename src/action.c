#include "action.h"

#include <stdlib.h>

#include "editor.h"
#include "terminal.h"

void editorUndo() {
    if (E.action_current == &E.action_head)
        return;

    editorDeleteText(E.action_current->action->added_range);
    editorPasteText(&E.action_current->action->deleted_text,
                    E.action_current->action->deleted_range.start_x,
                    E.action_current->action->deleted_range.start_y);
    E.action_current = E.action_current->prev;
    E.dirty--;
}

void editorRedo() {
    if (!E.action_current->next)
        return;

    E.action_current = E.action_current->next;
    editorDeleteText(E.action_current->action->deleted_range);
    editorPasteText(&E.action_current->action->added_text,
                    E.action_current->action->added_range.start_x,
                    E.action_current->action->added_range.start_y);
    E.dirty++;
}

void editorAppendAction(EditorAction* action) {
    if (!action)
        return;

    EditorActionList* node = malloc(sizeof(EditorActionList));
    if (!node)
        PANIC("malloc");
    node->action = action;
    node->next = NULL;

    E.dirty++;

    if (E.action_current == &E.action_head) {
        E.action_head.next = node;
        node->prev = &E.action_head;
        E.action_current = node;
        return;
    }
    node->prev = E.action_current;
    editorFreeActionList(E.action_current->next);
    E.action_current->next = node;
    E.action_current = E.action_current->next;
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