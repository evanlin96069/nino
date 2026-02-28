#ifndef CONFIG_H
#define CONFIG_H

#include "highlight.h"

#define EDITOR_CONFIG_EXT "." EDITOR_NAME
#define EDITOR_RC_FILE EDITOR_NAME "rc"

#define EDITOR_COLOR_COUNT 34

typedef struct {
    const char* label;
    Color* color;
} ColorElement;

extern const ColorElement color_element_map[EDITOR_COLOR_COUNT];

#define CONVAR(_name, _help_string, _default_string, _callback) \
    EditorConCmd cvar_##_name = {                               \
        .name = #_name,                                         \
        .help_string = _help_string,                            \
        .cvar = {.default_string = _default_string, .callback = _callback}}

#define CON_COMMAND(_name, _help_string)                        \
    static void _name##_callback(void);                         \
    EditorConCmd ccmd_##_name = {.name = #_name,                \
                                 .help_string = _help_string,   \
                                 .callback = _name##_callback}; \
    void _name##_callback(void)

#define CVAR(name) cvar_##name
#define CCMD(name) ccmd_##name
#define INIT_CONVAR(name) editorInitConVar(&CVAR(name))
#define INIT_CONCOMMAND(name) editorInitConCmd(&ccmd_##name)

#define EXTERN_CONVAR(name) extern EditorConCmd CVAR(name)

#define CONVAR_GETINT(name) CVAR(name).cvar.int_val
#define CONVAR_GETSTR(name) CVAR(name).cvar.string_val
#define CONVAR_SETINT(name, val) editorSetConVarInt(&CVAR(name).cvar, val, true)
#define CONVAR_SETSTR(name, val) editorSetConVar(&CVAR(name).cvar, val, true)

#define COMMAND_MAX_ARGC 64
#define COMMAND_MAX_LENGTH 512

typedef struct EditorConCmdArgs {
    int argc;
    char* argv[COMMAND_MAX_ARGC];
} EditorConCmdArgs;

extern EditorConCmdArgs args;

typedef void (*CommandCallback)(void);
typedef void (*ConVarCallback)(void);

typedef struct EditorConVar {
    const char* default_string;
    char string_val[COMMAND_MAX_LENGTH];
    int int_val;
    ConVarCallback callback;
} EditorConVar;

typedef struct EditorConCmd {
    struct EditorConCmd* next;
    const char* name;
    const char* help_string;
    bool has_callback;
    union {
        CommandCallback callback;
        EditorConVar cvar;
    };
} EditorConCmd;

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

EXTERN_CONVAR(tabsize);
EXTERN_CONVAR(whitespace);
EXTERN_CONVAR(autoindent);
EXTERN_CONVAR(backspace);
EXTERN_CONVAR(bracket);
EXTERN_CONVAR(trailing);
EXTERN_CONVAR(drawspace);
EXTERN_CONVAR(syntax);
EXTERN_CONVAR(helpinfo);
EXTERN_CONVAR(ignorecase);
EXTERN_CONVAR(mouse);
EXTERN_CONVAR(osc52_copy);
EXTERN_CONVAR(ex_default_width);
EXTERN_CONVAR(ex_show_hidden);
EXTERN_CONVAR(newline_default);
EXTERN_CONVAR(ttimeoutlen);
EXTERN_CONVAR(lineno);
EXTERN_CONVAR(readonly);

void editorRegisterCommands(void);
void editorUnregisterCommands(void);
bool editorLoadConfig(const char* path);
void editorLoadInitConfig(void);
void editorCmd(const char* command);
void editorOpenConfigPrompt(void);

void editorSetConVar(EditorConVar* thisptr,
                     const char* string_val,
                     bool trigger_cb);
void editorSetConVarInt(EditorConVar* thisptr, int int_val, bool trigger_cb);
void editorInitConCmd(EditorConCmd* thisptr);
void editorInitConVar(EditorConCmd* thisptr);
EditorConCmd* editorFindCmd(const char* name);

int editorGetDefaultNewline(void);
int editorGetLinenoWidth(const EditorFile* file);

#endif
