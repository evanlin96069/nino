#ifndef CONFIG_H
#define CONFIG_H

typedef struct EditorConfig {
    int tab_size;
} EditorConfig;

void editorLoadConfig(const char* filename);
void editorSetting();

#endif
