#ifndef CONFIG_H
#define CONFIG_H

#include "defines.h"
#include "utils.h"

typedef struct EditorConfig {
    int tab_size;
    Color status_color[2];
    Color highlight_color[HL_TYPE_COUNT];
} EditorConfig;

void editorLoadConfig();
void editorSetting();

#endif
