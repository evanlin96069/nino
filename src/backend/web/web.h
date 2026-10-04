#ifndef WEB_H
#define WEB_H

#include <emscripten.h>
#include <emscripten/html5.h>

#include "backend/shared/event.h"

void webInit(void);
void webFree(void);

void webRefreshScreen(bool force_redraw);
void webDispatchEvent(Event event);

#endif
