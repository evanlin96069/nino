#ifndef INPUT_H
#define INPUT_H

void editorMoveCursor(int key);
void editorProcessKeypress(void);

void editorScrollToCursor(void);
void editorScrollToCursorCenter(void);
void editorScroll(int dist);

void mousePosToEditorPos(int* x, int* y);
int getMousePosField(int x, int y);

#endif
