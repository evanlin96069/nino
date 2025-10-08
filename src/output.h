#ifndef OUTPUT_H
#define OUTPUT_H

#define LINENO_WIDTH() (CONVAR_GETINT(lineno) ? gCurFile->lineno_width : 0)

void editorRefreshScreen(void);

#endif
