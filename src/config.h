#ifndef CONFIG_H
#define CONFIG_H

#include "highlight.h"

#define EDITOR_CONFIG_EXT "." EDITOR_NAME
#define EDITOR_RC_FILE EDITOR_NAME "rc"

// Commands

#define CON_COMMAND(_name, _help_string) \
    static void _name##_callback(void);  \
    ConCommand _name = {                 \
        .name = #_name,                  \
        .help_string = _help_string,     \
        .is_command = true,              \
        .callback = _name##_callback,    \
    };                                   \
    void _name##_callback(void)

#define _CONVAR8(_name, _default_string, _help_string, _has_min, _min_value, \
                 _has_max, _max_value, _change_callback)                     \
    extern ConVar _name;                                                     \
    static void _name##_SetInt(int val) {                                    \
        editorSetConVarInt(&_name, val, true);                               \
    }                                                                        \
    static void _name##_SetString(const char* val) {                         \
        editorSetConVar(&_name, val, true);                                  \
    }                                                                        \
    ConVar _name = {                                                         \
        .name = #_name,                                                      \
        .help_string = _help_string,                                         \
        .is_command = false,                                                 \
        .default_string = _default_string,                                   \
        .has_min = _has_min,                                                 \
        .min_value = _min_value,                                             \
        .has_max = _has_max,                                                 \
        .max_value = _max_value,                                             \
        .change_callback = _change_callback,                                 \
        .setInt = _name##_SetInt,                                            \
        .setString = _name##_SetString,                                      \
    }

#define _CONVAR7(name, default_string, help_string, has_min, min_value,      \
                 has_max, max_value)                                         \
    _CONVAR8(name, default_string, help_string, has_min, min_value, has_max, \
             max_value, NULL)

#define _CONVAR4(name, default_string, help_string, change_callback) \
    _CONVAR8(name, default_string, help_string, false, 0, false, 0,  \
             change_callback)

#define _CONVAR3(name, default_string, help_string) \
    _CONVAR8(name, default_string, help_string, false, 0, false, 0, NULL)

#define _CONVAR2(name, default_string) \
    _CONVAR8(name, default_string, "", false, 0, false, 0, NULL)

#define _CONVAR1(name) _CONVAR8(name, "", "", false, 0, false, 0, NULL)

#define _GET_CONVAR_MACRO(_1, _2, _3, _4, _5, _6, _7, _8, NAME, ...) NAME
#define CONVAR(...)                                                            \
    _GET_CONVAR_MACRO(__VA_ARGS__, _CONVAR8, _CONVAR7, , , _CONVAR4, _CONVAR3, \
                      _CONVAR2, _CONVAR1)                                      \
    (__VA_ARGS__)

#define BASE_FIELDS              \
    struct ConCommandBase* next; \
    const char* name;            \
    const char* help_string;     \
    bool is_command

typedef struct ConCommandBase {
    BASE_FIELDS;
} ConCommandBase;

#define COMMAND_MAX_ARGC 64
#define COMMAND_MAX_LENGTH 512

typedef struct CCommand {
    int argc;
    char* argv[COMMAND_MAX_ARGC];
} CCommand;

extern CCommand args;

typedef void (*CommandCallback)(void);
typedef void (*ChangeCallback)(void);

typedef struct ConVar {
    BASE_FIELDS;

    const char* default_string;
    char string_value[COMMAND_MAX_LENGTH];
    int int_value;

    bool has_min;
    int min_value;
    bool has_max;
    int max_value;

    ChangeCallback change_callback;

    void (*setInt)(int val);
    void (*setString)(const char* val);
} ConVar;

typedef struct ConCommand {
    BASE_FIELDS;
    CommandCallback callback;
} ConCommand;

#undef BASE_FIELDS

extern ConVar tabsize;
extern ConVar whitespace;
extern ConVar autoindent;
extern ConVar backspace;
extern ConVar bracket;
extern ConVar trailing;
extern ConVar drawspace;
extern ConVar syntax;
extern ConVar helpinfo;
extern ConVar intro;
extern ConVar start_new_file;
extern ConVar ignorecase;
extern ConVar mouse;
extern ConVar osc52_copy;
extern ConVar ex_default_width;
extern ConVar ex_show_hidden;
extern ConVar newline_default;
extern ConVar ttimeoutlen;
extern ConVar lineno;
extern ConVar readonly;
extern ConVar shell;

// Color scheme

#define EDITOR_COLOR_COUNT 34

typedef struct {
    const char* label;
    Color* color;
} ColorElement;

extern const ColorElement color_element_map[EDITOR_COLOR_COUNT];

typedef struct EditorColorScheme {
    Color bg;
    Color top_status[6];
    Color explorer[5];
    Color prompt[2];
    Color status[6];
    Color line_number[2];
    Color cursor_line;
    Color highlightFg[HL_FG_COUNT];
    Color highlightBg[HL_BG_COUNT];
} EditorColorScheme;

extern const EditorColorScheme color_default;

void editorRegisterCommands(void);
void editorUnregisterCommands(void);
bool editorLoadConfig(const char* path);
void editorLoadInitConfig(void);
void editorCmd(const char* command);
void editorOpenConfigPrompt(void);

void editorSetConVar(ConVar* thisptr, const char* string_val, bool trigger_cb);
void editorSetConVarInt(ConVar* thisptr, int int_val, bool trigger_cb);
void editorInitConCommand(ConCommand* thisptr);
void editorInitConVar(ConVar* thisptr);
ConCommandBase* editorFindCmd(const char* name);

int editorGetDefaultNewline(void);
int editorGetLinenoWidth(const EditorFile* file);

#endif
