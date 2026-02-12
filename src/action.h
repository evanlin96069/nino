#ifndef ACTION_H
#define ACTION_H

#include "select.h"

typedef struct EditorCursor {
    int x, y;
    bool is_selected;
    int select_x;
    int select_y;
} EditorCursor;

typedef struct Edit {
    int x, y;
    EditorClipboard before;
    EditorClipboard after;
} Edit;

typedef struct EditAction {
    Edit data;
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

void editorApplyEdit(Edit* edit, bool undo);
bool editorUndo(void);
bool editorRedo(void);
void editorAppendAction(EditorAction* action);
void editorFreeActionList(EditorActionList* thisptr);
void editorFreeAction(EditorAction* action);

#endif
