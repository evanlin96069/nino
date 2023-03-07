#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdbool.h>

#include "editor.h"

bool editorOpen(EditorFile* file, char* filename);
void editorSave(EditorFile* file, int save_as);

#endif
