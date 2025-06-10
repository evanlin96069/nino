#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

#include "defines.h"
#include "utils.h"

typedef struct EditorConCmd EditorConCmd;

extern EditorConCmd cvar_tabsize;
extern EditorConCmd cvar_whitespace;
extern EditorConCmd cvar_autoindent;
extern EditorConCmd cvar_backspace;
extern EditorConCmd cvar_bracket;
extern EditorConCmd cvar_trailing;
extern EditorConCmd cvar_drawspace;
extern EditorConCmd cvar_syntax;
extern EditorConCmd cvar_helpinfo;
extern EditorConCmd cvar_ignorecase;
extern EditorConCmd cvar_mouse;
extern EditorConCmd cvar_osc52_copy;
extern EditorConCmd cvar_ex_default_width;
extern EditorConCmd cvar_ex_show_hidden;
extern EditorConCmd cvar_newline_default;

typedef struct EditorColorScheme EditorColorScheme;
extern const EditorColorScheme color_default;

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

#define INIT_CONVAR(name) editorInitConVar(&cvar_##name)
#define INIT_CONCOMMAND(name) editorInitConCmd(&ccmd_##name)

#define CONVAR_GETINT(name) cvar_##name.cvar.int_val
#define CONVAR_GETSTR(name) cvar_##name.cvar.string_val

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

struct EditorConCmd {
    struct EditorConCmd* next;
    const char* name;
    const char* help_string;
    bool has_callback;
    union {
        CommandCallback callback;
        EditorConVar cvar;
    };
};

struct EditorColorScheme {
    Color bg;
    Color top_status[6];
    Color explorer[5];
    Color prompt[2];
    Color status[6];
    Color line_number[2];
    Color cursor_line;
    Color highlightFg[HL_FG_COUNT];
    Color highlightBg[HL_BG_COUNT];
};

void editorInitConfig(void);
void editorFreeConfig(void);
bool editorLoadConfig(const char* path);
void editorOpenConfigPrompt(void);

void editorSetConVar(EditorConVar* thisptr, const char* string_val);
void editorInitConCmd(EditorConCmd* thisptr);
void editorInitConVar(EditorConCmd* thisptr);
EditorConCmd* editorFindCmd(const char* name);

int editorGetDefaultNewline(void);

#endif
