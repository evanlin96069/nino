#ifndef TERMINAL_H
#define TERMINAL_H

#define PANIC(s) panic(__FILE__, __LINE__, s)
void panic(const char* file, int line, const char* s);

void editorInitTerminal(void);
int editorReadKey(int* x, int* y);

void enableMouse(void);
void disableMouse(void);

void resizeWindow(void);
void terminalExit(void);

#endif
