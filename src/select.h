#ifndef SELECT_H
#define SELECT_H

#include "utils.h"

typedef struct EditorFile EditorFile;
typedef struct EditorCursor EditorCursor;

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

void getSelectStartEnd(const EditorCursor* cursor, EditorSelectRange* range);
bool isPosSelected(int row, int col, EditorSelectRange range);
EditorSelectRange getClipboardRange(int x,
                                    int y,
                                    const EditorClipboard* clipboard);

void editorDeleteText(EditorFile* file, EditorSelectRange range);
void editorCopyText(EditorFile* file,
                    EditorClipboard* clipboard,
                    EditorSelectRange range);
void editorCopyLine(EditorFile* file, EditorClipboard* clipboard, int row);
void editorPasteText(EditorFile* file,
                     const EditorClipboard* clipboard,
                     int x,
                     int y);

void editorFreeClipboardContent(EditorClipboard* clipboard);

void editorCopyToSysClipboard(EditorClipboard* clipboard, uint8_t newline);

void editorClipboardAppendAt(EditorClipboard* clipboard,
                             size_t line_index,
                             const char* data,
                             size_t len);
void editorClipboardAppendAtRepeat(EditorClipboard* clipboard,
                                   size_t line_index,
                                   char value,
                                   size_t count);
void editorClipboardAppendChar(EditorClipboard* clipboard, char c);
void editorClipboardAppendUnicode(EditorClipboard* clipboard, uint32_t unicode);
void editorClipboardAppendNewline(EditorClipboard* clipboard);

#endif
