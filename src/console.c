#include "console.h"

#include <stdarg.h>

#include "config.h"
#include "editor.h"

static void editorVMsg(const char* fmt, va_list ap) {
    vsnprintf(gEditor.con_msg[gEditor.con_rear], sizeof(gEditor.con_msg[0]),
              fmt, ap);

    if (gEditor.con_front == gEditor.con_rear) {
        gEditor.con_front = (gEditor.con_front + 1) % EDITOR_CON_COUNT;
        gEditor.con_size--;
    } else if (gEditor.con_front == -1) {
        gEditor.con_front = 0;
    }
    gEditor.con_size++;
    gEditor.con_rear = (gEditor.con_rear + 1) % EDITOR_CON_COUNT;
}

void editorMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    editorVMsg(fmt, ap);
    va_end(ap);
}

void editorDevMsg(const char* fmt, ...) {
    if (!developer.int_value)
        return;

    va_list ap;
    va_start(ap, fmt);
    editorVMsg(fmt, ap);
    va_end(ap);
}

void editorMsgClear(void) {
    gEditor.con_front = -1;
    gEditor.con_rear = 0;
    gEditor.con_size = 0;
}
