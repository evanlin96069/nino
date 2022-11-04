#ifndef SELECT_H
#define SELECT_H

#include <stddef.h>

typedef struct EditorClipboard {
    size_t size;
    char** data;
} EditorClipboard;

void getSelectStartEnd(int* start_x, int* start_y, int* end_x, int* end_y);
void editorSelectText();
void editorDeleteSelectText();
void editorCopySelectText();
void editorPasteText();
void editorFreeClipboard(EditorClipboard* clipboard);

#endif
