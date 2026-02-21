#ifndef INPUT_H
#define INPUT_H

typedef struct EditorTab EditorTab;

enum EditorField {
    FIELD_EMPTY,
    FIELD_TOP_STATUS,
    FIELD_TEXT,
    FIELD_LINENO,
    FIELD_PROMPT,
    FIELD_STATUS,
    FIELD_EXPLORER,
    FIELD_SPLIT_SEPARATOR,
    FIELD_ERROR,
};

void editorProcessKeypress(void);

void editorScrollToCursor(int split_index);
void editorScrollToCursorCenter(int split_index);
void editorScroll(int split_index, int dist);
void editorExplorerScroll(int dist);

void editorMousePosToEditorPos(int split_index,
                               int mouse_x,
                               int mouse_y,
                               int* out_x,
                               int* out_y);
int editorGetMousePosField(int x, int y, int* split_index);

void editorExplorerShow(void);

#endif
