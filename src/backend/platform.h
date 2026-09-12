#ifndef PLATFORM_H
#define PLATFORM_H

#include "utils/utils.h"

void platformPanic(const char* file, int line, const char* s);

void platformMouseEnable(void);
void platformMouseDisable(void);
void platformCopyToSysClipboard(StrView sv);

void platformSuspend(void);
void platformRunShell(const char* shell_hint, const char* cmd);

#endif
