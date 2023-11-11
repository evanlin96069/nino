#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>

typedef struct EditorInput {
    int type;
    union {
        uint32_t unicode;
        struct {
            int x;
            int y;
        } cursor;
    } data;
} EditorInput;

void editorInitTerminal(void);
EditorInput editorReadKey(void);

void enableMouse(void);
void disableMouse(void);

void resizeWindow(void);
void terminalExit(void);

#endif
