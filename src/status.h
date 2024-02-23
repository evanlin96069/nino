#ifndef STATUS_H
#define STATUS_H

#include "utils.h"

void editorMsg(const char* fmt, ...);
void editorMsgClear(void);

void editorSetPrompt(const char* fmt, ...);
void editorSetRightPrompt(const char* fmt, ...);

void editorDrawTopStatusBar(abuf* ab);
void editorDrawConMsg(abuf* ab);
void editorDrawPrompt(abuf* ab);
void editorDrawStatusBar(abuf* ab);

#endif
