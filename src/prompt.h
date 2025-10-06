#ifndef PROMPT_H
#define PROMPT_H

void editorMsg(const char* fmt, ...);
void editorMsgClear(void);

char* editorPrompt(const char* prompt, int state, void (*callback)(char*, int));
void editorGotoLine(void);
void editorFind(void);

#endif
