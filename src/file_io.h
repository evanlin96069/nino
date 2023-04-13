#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>

#include "editor.h"

bool editorOpen(EditorFile* file, const char* filename);
void editorSave(EditorFile* file, int save_as);
void editorOpenFilePrompt();

#endif
