#ifndef STATUS_H
#define STATUS_H

#include "utils.h"

void editorSetStatusMsg(const char* fmt, ...);
void editorSetRStatusMsg(const char* fmt, ...);
void editorDrawTopStatusBar(abuf* ab);
void editorDrawPrompt(abuf* ab);
void editorDrawStatusBar(abuf* ab);

#endif
