#ifndef TERMINAL_H
#define TERMINAL_H

#define ANSI_CLEAR_STYLE "\x1b[m"
#define ANSI_ERASE_LINE "\x1b[K"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_NOT_UNDERLINE "\x1b[24m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_NOT_INVERT "\x1b[27m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

#define ANSI_CURSOR_RESET_POS "\x1b[H"
#define ANSI_CURSOR_SHOW "\x1b[?25h"
#define ANSI_CURSOR_HIDE "\x1b[?25l"

#define ANSI_SYNC_BEGIN "\x1b[?2026h"
#define ANSI_SYNC_END "\x1b[?2026l"

#define ANSI_FOCUS_ENABLE "\x1b[?1004h"
#define ANSI_FOCUS_DISABLE "\x1b[?1004l"
#define ANSI_SWAP_ENABLE "\x1b[?1049h"
#define ANSI_SWAP_DISABLE "\x1b[?1049l"
#define ANSI_MOUSE_ENABLE "\x1b[?1000h\x1b[?1002h\x1b[?1006h"
#define ANSI_MOUSE_DISABLE "\x1b[?1007l\x1b[?1006l\x1b[?1002l\x1b[?1000l"
#define ANSI_BRACKETED_PASTE_ENABLE "\x1b[?2004h"
#define ANSI_BRACKETED_PASTE_DISABLE "\x1b[?2004l"

void terminalInit(void);
void terminalFree(void);

void terminalStart(void);
void terminalExit(void);

void terminalProcessInput(void);
void terminalRefreshScreen(bool force_redraw);

#endif
