#ifndef PROMPT_H
#define PROMPT_H

char* editorPrompt(char* prompt, int state, void (*callback)(char*, int));
void editorGotoLine(void);
void editorFind(void);

#endif
