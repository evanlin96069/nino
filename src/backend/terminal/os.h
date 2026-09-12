#ifndef TERM_OS_H
#define TERM_OS_H

#include "backend/shared/event.h"

void terminalOsInit(void);
void terminalOsDeinit(void);

// Terminal
void enableRawMode(void);
void disableRawMode(void);
bool isStdinTty(void);

typedef enum {
    CONSOLE_EVENT_ERROR = -2,
    CONSOLE_EVENT_TIMEOUT = -1,
    CONSOLE_EVENT_KEY,
    CONSOLE_EVENT_RESIZE,
} ConsoleEventType;

typedef ResizeEvent ConsoleResizeEvent;

typedef struct {
    ConsoleEventType type;
    union {
        uint32_t unicode;
        ConsoleResizeEvent resize;
    } data;
} ConsoleEvent;

ConsoleEvent readConsoleEvent(int timeout_ms);
int getWindowSize(int* width, int* height);

int writeConsole(const void* buf, size_t count);

#define writeConsoleStr(s) writeConsole((s), sizeof(s) - 1)
static inline bool writeConsoleAll(const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    while (len) {
        int n = writeConsole(p, len);
        if (n <= 0)
            return false;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}

#endif
