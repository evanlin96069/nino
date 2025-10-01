#ifndef ACTION_H
#define ACTION_H

#include "select.h"

typedef struct EditorCursor {
    int x, y;
    bool is_selected;
    int select_x;
    int select_y;
} EditorCursor;

typedef struct EditAction {
    EditorSelectRange deleted_range;
    EditorClipboard deleted_text;

    EditorSelectRange added_range;
    EditorClipboard added_text;

    EditorCursor old_cursor;
    EditorCursor new_cursor;
} EditAction;

typedef struct AttributeAction {
    int old_newline;
    int new_newline;
} AttributeAction;

typedef enum EditorActionType {
    ACTION_EDIT,
    ACTION_ATTRI,
} EditorActionType;

typedef struct EditorAction {
    EditorActionType type;
    union {
        EditAction edit;
        AttributeAction attri;
    };
} EditorAction;

typedef struct EditorActionList {
    struct EditorActionList* prev;
    struct EditorActionList* next;
    EditorAction* action;
} EditorActionList;

bool editorUndo(void);
bool editorRedo(void);
void editorAppendAction(EditorAction* action);
void editorFreeActionList(EditorActionList* thisptr);
void editorFreeAction(EditorAction* action);

#endif
