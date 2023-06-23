#ifndef TERMINAL_H
#define TERMINAL_H

#define PANIC(s) panic(__FILE__, __LINE__, s)
void panic(char* file, int line, const char* s);

void enableRawMode(void);
int editorReadKey(int* x, int* y);
int getWindowSize(int* rows, int* cols);
void enableSwap(void);
void disableSwap(void);
void enableMouse(void);
void disableMouse(void);

#ifndef _WIN32
void enableAutoResize(void);
#endif

void resizeWindow(void);
void terminalExit(void);

#endif
