#include "event.h"

PasteEvent pasteEventCreate(StrView content) {
    VECTOR(Str) lines = {0};

    bool last_was_cr = false;
    StrView line = {0};

    for (uint32_t i = 0; i < content.size; i++) {
        char c = content.data[i];
        if (c == '\r' || c == '\n') {
            if (c == '\n' && last_was_cr) {
                last_was_cr = false;
                continue;
            }

            last_was_cr = (c == '\r');

            vector_push(&lines, strCopy(line));
            line.size = 0;
            line.data = NULL;
        } else {
            if (!line.data) {
                line.data = &content.data[i];
            }
            line.size++;
        }
    }

    if (line.size > 0) {
        vector_push(&lines, strCopy(line));
    }

    // TODO: Just make EditorClipboard VECTOR(Str)
    return (EditorClipboard){
        .size = lines.size,
        .lines = lines.data,
    };
}

void pasteEventFree(PasteEvent* e) {
    editorFreeClipboardContent(e);
}
