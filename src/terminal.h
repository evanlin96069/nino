#ifndef TERMINAL_H
#define TERMINAL_H

#include "select.h"

// ANSI escape sequences
#define ANSI_CLEAR "\x1b[m"
#define ANSI_ERASE_LINE "\x1b[K"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_NOT_UNDERLINE "\x1b[24m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_NOT_INVERT "\x1b[27m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

#define ANSI_CURSOR_RESET_POS "\x1b[H"
#define ANSI_CURSOR_SHOW "\x1b[?25h"
#define ANSI_CURSOR_HIDE "\x1b[?25l"

// Keys
#define CTRL_KEY(k) ((k)&0x1F)
#define ALT_KEY(k) ((k) | 0x1B00)

enum EditorKey {
    UNKNOWN = -1,
    ESC = 27,
    BACKSPACE = 127,
    CHAR_INPUT = 1000,
    PASTE_INPUT,
    ARROW_UP,
    ARROW_DOWN,
    ARROW_LEFT,
    ARROW_RIGHT,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN,
    SHIFT_UP,
    SHIFT_DOWN,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    SHIFT_HOME,
    SHIFT_END,
    SHIFT_PAGE_UP,
    SHIFT_PAGE_DOWN,
    ALT_UP,
    ALT_DOWN,
    SHIFT_ALT_UP,
    SHIFT_ALT_DOWN,
    CTRL_UP,
    CTRL_DOWN,
    CTRL_LEFT,
    CTRL_RIGHT,
    CTRL_HOME,
    CTRL_END,
    CTRL_PAGE_UP,
    CTRL_PAGE_DOWN,
    SHIFT_CTRL_UP,
    SHIFT_CTRL_DOWN,
    SHIFT_CTRL_LEFT,
    SHIFT_CTRL_RIGHT,
    SHIFT_CTRL_PAGE_UP,
    SHIFT_CTRL_PAGE_DOWN,
    MOUSE_PRESSED,
    MOUSE_RELEASED,
    SCROLL_PRESSED,
    SCROLL_RELEASED,
    MOUSE_MOVE,
    WHEEL_UP,
    WHEEL_DOWN,
};

typedef struct EditorInput {
    int type;
    union {
        uint32_t unicode;
        struct {
            int x;
            int y;
        } cursor;
        EditorClipboard paste;
    } data;
} EditorInput;

void editorInitTerminal(void);
EditorInput editorReadKey(void);
void editorFreeInput(EditorInput* input);

void enableMouse(void);
void disableMouse(void);

void setWindowSize(int rows, int cols);
void resizeWindow(void);

void terminalExit(void);

#endif
