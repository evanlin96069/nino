#ifndef TERMINAL_H
#define TERMINAL_H

#define PANIC(s) panic(__FILE__, __LINE__, s)
void panic(char* file, int line, const char* s);

void enableRawMode();
int editorReadKey(int* x, int* y);
int getWindowSize(int* rows, int* cols);
void enableSwap();
void disableSwap();
void enableMouse();
void disableMouse();

#ifndef _WIN32
void enableAutoResize();
#endif

void resizeWindow();
void terminalExit();

#endif
