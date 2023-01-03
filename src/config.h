#ifndef CONFIG_H
#define CONFIG_H

#include "defines.h"
#include "utils.h"

typedef struct EditorConCmd EditorConCmd;

extern EditorConCmd cvar_tabsize;
extern EditorConCmd cvar_whitespace;
extern EditorConCmd cvar_autoindent;
extern EditorConCmd cvar_syntax;
extern EditorConCmd cvar_helpinfo;
extern EditorConCmd cvar_mouse;
extern EditorConCmd ccmd_color;

#define CONVAR(_name, _help_string, _default_string)          \
    EditorConCmd cvar_##_name = {.name = #_name,              \
                                 .help_string = _help_string, \
                                 .cvar = {.default_string = _default_string}}

#define CON_COMMAND(_name, _help_string)                                       \
    static int _name##_callback(EditorConCmd* thisptr, EditorConCmdArgs args); \
    EditorConCmd ccmd_##_name = {.name = #_name,                               \
                                 .help_string = _help_string,                  \
                                 .callback = _name##_callback};                \
    int _name##_callback(EditorConCmd* thisptr, EditorConCmdArgs args)

#define INIT_CONVAR(name) editorInitConVar(&cvar_##name)
#define INIT_CONCOMMAND(name) editorInitConCmd(&ccmd_##name)

#define CONVAR_GETINT(name) cvar_##name.cvar.int_val
#define CONVAR_GETSTR(name) cvar_##name.cvar.string_val

#define COMMAND_MAX_ARGC 16
#define COMMAND_MAX_LENGTH 64

typedef struct EditorConCmdArgs {
    int argc;
    char argv[COMMAND_MAX_ARGC][COMMAND_MAX_LENGTH];
} EditorConCmdArgs;

typedef int (*CommandCallback)(EditorConCmd* thisptr, EditorConCmdArgs args);

typedef struct EditorConVar {
    const char* default_string;
    char string_val[COMMAND_MAX_LENGTH];
    int int_val;
} EditorConVar;

struct EditorConCmd {
    struct EditorConCmd* next;
    const char* name;
    const char* help_string;
    int has_callback : 1;
    union {
        CommandCallback callback;
        EditorConVar cvar;
    };
};

typedef struct EditorColorConfig {
    Color status[2];
    Color highlight[HL_TYPE_COUNT];
} EditorColorConfig;

void editorInitCommands();
void editorLoadConfig();
void editorSetting();

void editorSetConVar(EditorConVar* thisptr, const char* string_val);
void editorInitConCmd(EditorConCmd* thisptr);
void editorInitConVar(EditorConCmd* thisptr);
EditorConCmd* editorFindCmd(const char* name);

#endif
