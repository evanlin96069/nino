#ifndef SELECT_H
#define SELECT_H

#include "utils.h"

typedef struct EditorClipboard {
    size_t size;
    Str* lines;
} EditorClipboard;

typedef struct EditorSelectRange {
    int start_x;
    int start_y;
    int end_x;
    int end_y;
} EditorSelectRange;

void getSelectStartEnd(EditorSelectRange* range);
bool isPosSelected(int row, int col, EditorSelectRange range);

void editorDeleteText(EditorSelectRange range);
void editorCopyText(EditorClipboard* clipboard, EditorSelectRange range);
void editorCopyLine(EditorClipboard* clipboard, int row);
void editorPasteText(const EditorClipboard* clipboard, int x, int y);

void editorFreeClipboardContent(EditorClipboard* clipboard);

void editorCopyToSysClipboard(EditorClipboard* clipboard, uint8_t newline);

#endif
