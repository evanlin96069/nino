#include "web.h"

#include "editor/editor.h"

#include "utils/os.h"
#include "utils/utils.h"

static bool mouse_enabled = true;

void platformMouseEnable(void) {
    mouse_enabled = true;
}

void platformMouseDisable(void) {
    mouse_enabled = false;
}

// clang-format off

EM_JS(void, webReload, (void), {
    window.location.reload();
})

// clang-format on

void webDispatchEvent(Event event) {
    editorProcessEvent(event, (uint64_t)getTimeMs());
    if (gEditor.state == STATE_EXIT) {
        // webFree();
        // editorFree();
        webReload();
    } else {
        webRefreshScreen(false);
    }
}

EMSCRIPTEN_KEEPALIVE void webEventKey(int code,
                                      int modifiers,
                                      int id,
                                      uint32_t unicode,
                                      int kind) {
    Event event = {
        .type = EVENT_KEY,
        .key =
            {
                .modifiers = (uint8_t)modifiers,
                .code = (uint8_t)code,
                .id = (uint8_t)id,
                .unicode = unicode,
                .kind = (uint8_t)kind,
            },
    };
    webDispatchEvent(event);
}

EMSCRIPTEN_KEEPALIVE void webEventMouse(int type, int x, int y) {
    if (!mouse_enabled)
        return;

    Event event = {
        .type = EVENT_MOUSE,
        .mouse =
            {
                .type = (MouseEventType)type,
                .x = x,
                .y = y,
            },
    };
    webDispatchEvent(event);
}

EMSCRIPTEN_KEEPALIVE void webEventPaste(const char* text, int length) {
    StrView content = {
        .data = text,
        .size = (size_t)length,
    };
    Event event = {
        .type = EVENT_PASTE,
        .paste = pasteEventCreate(content),
    };
    webDispatchEvent(event);
    pasteEventFree(&event.paste);
}

EMSCRIPTEN_KEEPALIVE void webEventFocus(int focused) {
    Event event = {
        .type = focused ? EVENT_FOCUS_GAINED : EVENT_FOCUS_LOST,
    };
    webDispatchEvent(event);
}

EMSCRIPTEN_KEEPALIVE void webEventResize(int width, int height) {
    Event event = {
        .type = EVENT_RESIZE,
        .resize =
            {
                .width = width,
                .height = height,
            },
    };
    webDispatchEvent(event);
}
