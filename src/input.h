#ifndef INPUT_H
#define INPUT_H

char* editorPrompt(char* prompt, int state, void (*callback)(char*, int));
void editorMoveCursor(int key);
void editorProcessKeypress();

#endif
