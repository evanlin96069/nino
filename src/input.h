#ifndef INPUT_H
#define INPUT_H

enum EditorField {
    FIELD_EMPTY,
    FIELD_TOP_STATUS,
    FIELD_TEXT,
    FIELD_LINENO,
    FIELD_PROMPT,
    FIELD_STATUS,
    FIELD_EXPLORER,
    FIELD_ERROR,
};

void editorMoveCursor(int key);
void editorProcessKeypress(void);

void editorScrollToCursor(void);
void editorScrollToCursorCenter(void);
void editorScroll(int dist);

void mousePosToEditorPos(int* x, int* y);
int getMousePosField(int x, int y);

void editorExplorerShow(void);

#endif
