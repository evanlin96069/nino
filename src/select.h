#ifndef SELECT_H
#define SELECT_H

#include <stddef.h>

typedef struct EditorClipboard {
    size_t size;
    char** data;
} EditorClipboard;

typedef struct EditorSelectRange {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
} EditorSelectRange;

void getSelectStartEnd(EditorSelectRange* range);
void editorSelectText();

void editorDeleteText(EditorSelectRange range);
void editorCopyText(EditorClipboard* clipboard, EditorSelectRange range);
void editorPasteText(const EditorClipboard* clipboard, int x, int y);

void editorFreeClipboardContent(EditorClipboard* clipboard);

#endif
