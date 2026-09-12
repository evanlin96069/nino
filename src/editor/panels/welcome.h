#ifndef PANEL_WELCOME_H
#define PANEL_WELCOME_H

#include "ui/panel.h"

typedef struct WelcomePanel {
    Panel base;
} WelcomePanel;

WelcomePanel* panelWelcomeCreate(void);

#endif
