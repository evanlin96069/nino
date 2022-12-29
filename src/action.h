#ifndef ACTION_H
#define ACTION_H

#include "select.h"

typedef struct EditorCursor {
    int x, y;
    int is_selected : 1;
    int select_x;
    int select_y;
} EditorCursor;

typedef struct EditorAction {
    EditorSelectRange deleted_range;
    EditorClipboard deleted_text;

    EditorSelectRange added_range;
    EditorClipboard added_text;

    EditorCursor old_cursor;
    EditorCursor new_cursor;
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