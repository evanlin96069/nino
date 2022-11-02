#ifndef TERMINAL_H
#define TERMINAL_H

#define DIE(s) die(__FILE__, __LINE__, s)
void die(char* file, int line, const char* s);

void enableRawMode();
int editorReadKey(int* x, int* y);
int getWindowSize(int* rows, int* cols);
void enableSwap();
void disableSwap();
void enableMouse();
void disableMouse();
void enableAutoResize();
void resizeWindow();

#endif
