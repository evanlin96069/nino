#ifndef ACTION_H
#define ACTION_H

#include "select.h"

typedef struct EditorAction {
    EditorSelectRange deleted_range;
    EditorClipboard deleted_text;
    EditorSelectRange added_range;
    EditorClipboard added_text;
} EditorAction;

typedef struct EditorActionList {
    struct EditorActionList* prev;
    struct EditorActionList* next;
    EditorAction* action;
} EditorActionList;

void editorUndo();
void editorRedo();
void editorAppendAction(EditorAction* action);
void editorFreeActionList(EditorActionList* thisptr);
void editorFreeAction(EditorAction* action);

#endif