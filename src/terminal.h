#ifndef TERMINAL_H
#define TERMINAL_H

#include "os.h"
#include "select.h"

// Input

typedef enum EventType {
    EVENT_ERROR = -2,
    EVENT_TIMEOUT = -1,
    EVENT_FOCUS_GAINED,
    EVENT_FOCUS_LOST,
    EVENT_KEY,
    EVENT_MOUSE,
    EVENT_PASTE,
    EVENT_RESIZE,
} EventType;

#define _KEY_EVENT3(modifiers, code, id) \
    ((uint32_t)(modifiers) | ((uint32_t)(code) << 8) | ((uint32_t)(id) << 16))

#define _KEY_EVENT2(modifiers, code) _KEY_EVENT3(modifiers, code, 0)

#define _KEY_EVENT1(code) _KEY_EVENT3(0, code, 0)

#define _GET_KEY_EVENT_MACRO(_1, _2, _3, NAME, ...) NAME
// _UNUSED to surpress zero variadic macro warning
#define KEY_EVENT(...)                                                       \
    _GET_KEY_EVENT_MACRO(__VA_ARGS__, _KEY_EVENT3, _KEY_EVENT2, _KEY_EVENT1, \
                         _UNUSED)                                            \
    (__VA_ARGS__)

enum KeyCode {
    KEY_BACKSPACE,
    KEY_ENTER,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_UP,
    KEY_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_TAB,
    KEY_DELETE,
    KEY_INSERT,
    KEY_F,
    KEY_ESC,
    KEY_CHAR,
    KEY_TEXT,
};

enum KeyModifiers {
    KEY_MOD_NONE = 0,
    KEY_MOD_SHIFT = 1 << 0,
    KEY_MOD_ALT = 1 << 1,
    KEY_MOD_CTRL = 1 << 2,
    KEY_MOD_META = 1 << 3,
};

enum KeyKind {
    KEY_KIND_PRESS,
    KEY_KIND_REPEAT,
    KEY_KIND_RELEASE,
};

typedef struct KeyEvent {
    union {
        struct {
            uint8_t modifiers;  // KeyModifiers
            uint8_t code;       // KeyCode
            uint8_t id;         // KEY_CHAR or KEY_F
            uint8_t kind;       // KeyKind, unused
        };
        uint32_t value;
    };
    uint32_t unicode;  // KEY_TEXT
} KeyEvent;

typedef enum MouseEventType {
    MOUSE1_PRESSED,
    MOUSE2_PRESSED,
    MOUSE3_PRESSED,
    MOUSE1_RELEASED,
    MOUSE2_RELEASED,
    MOUSE3_RELEASED,
    MOUSE1_DRAG,
    MOUSE2_DRAG,
    MOUSE3_DRAG,
    MWHEEL_UP,
    MWHEEL_DOWN,
} MouseEventType;

typedef struct MouseEvent {
    MouseEventType type;
    int x, y;
} MouseEvent;

typedef ConsoleResizeEvent ResizeEvent;

typedef EditorClipboard PasteEvent;

typedef struct Event {
    EventType type;
    union {
        KeyEvent key;
        MouseEvent mouse;
        PasteEvent paste;
        ResizeEvent resize;
    };
} Event;

void terminalInit(void);
void terminalStart(void);
void terminalExit(void);

Event eventPoll(int timeout_ms);
void eventFree(Event* event);

void enableMouse(void);
void disableMouse(void);

#endif
