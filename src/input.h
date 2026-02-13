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
    FIELD_ERROR,
};

void editorProcessKeypress(void);

void editorScrollToCursor(EditorTab* tab);
void editorScrollToCursorCenter(EditorTab* tab);
void editorScroll(EditorTab* tab, int dist);

void editorMousePosToEditorPos(int* x, int* y);
int editorGetMousePosField(int x, int y);

void editorExplorerShow(void);

#endif
