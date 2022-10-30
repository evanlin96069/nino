#ifndef CONFIG_H
#define CONFIG_H

#include "defines.h"
#include "utils.h"

typedef struct EditorConfig {
    Color status_color[2];
    Color highlight_color[HL_TYPE_COUNT];
    char tab_size;
    int whitespace : 1;
    int auto_indent : 1;
    int syntax : 1;
    int help_info : 1;
} EditorConfig;

void editorLoadConfig();
void editorSetting();

#endif
