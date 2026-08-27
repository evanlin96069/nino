#ifndef PANEL_PROMPT_H
#define PANEL_PROMPT_H

#include "row.h"

#include "ui/panel.h"

#define PROMPT_MAX_PREFIX 32
#define PROMPT_MAX_RIGHT 32

typedef enum PromptEventType {
    PROMPT_EVENT_KEY,
    PROMPT_EVENT_MOUSE,
    PROMPT_EVENT_CANCEL,
    PROMPT_EVENT_SUBMIT,
} PromptEventType;

typedef struct PromptEvent {
    PromptEventType type;
    const char* query;
    union {
        KeyEvent key_event;
        UIMouseEvent mouse_event;
    };
} PromptEvent;

typedef void (*PromptCallback)(PromptEvent event, void* user_data);

typedef struct PromptPanel {
    Panel base;

    char prompt_prefix[PROMPT_MAX_PREFIX];
    char prompt_right[PROMPT_MAX_RIGHT];
    int prompt_prefix_len;
    EditorRow prompt_row;

    int cx;
    int select_x;  // -1 if no selection

    PromptCallback callback;
    void* user_data;
} PromptPanel;

PromptPanel* panelPromptCreate(void);

void editorPrompt(const char* prefix, PromptCallback callback, void* user_data);
void editorSetRightPrompt(const char* fmt, ...);

#endif
