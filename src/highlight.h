#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include "editor.h"

void editorUpdateSyntax(EditorRow* row);
int editorSyntaxToColor(int hl);
void editorSelectSyntaxHighlight();

#endif
