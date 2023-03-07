#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "editor.h"

void editorUpdateSyntax(EditorFile* file, EditorRow* row);
void editorSelectSyntaxHighlight(EditorFile* file);

#endif
