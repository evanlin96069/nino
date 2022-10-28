#include "status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "utils.h"

void editorSetStatusMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
    va_end(ap);
}

void editorDrawTopStatusBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    abufAppend(ab, "\x1b[48;5;234m");
    char status[80];
    int len =
        snprintf(status, sizeof(status), "  Nino Editor v" EDITOR_VERSION);
    char rstatus[80];
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s%.20s", E.dirty ? "*" : "",
                        E.filename ? E.filename : "Untitled");
    if (len > cols)
        len = cols;
    abufAppendN(ab, status, len);

    for (int i = len; i < cols; i++) {
        if (i == (cols - rlen) / 2) {
            abufAppendN(ab, rstatus, rlen);
            i += rlen;
        }
        abufAppend(ab, " ");
    }
    abufAppend(ab, ANSI_CLEAR);
    abufAppend(ab, "\r\n");
}

void editorDrawStatusBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    char color[20];
    colorToANSI(E.cfg->status_color[0], color, 0);
    abufAppend(ab, color);
    colorToANSI(E.cfg->status_color[1], color, 1);
    abufAppend(ab, color);

    char rstatus[80];
    const char* help_info[] = {
        " ^Q: Quit  ^S: Save  ^F: Find  ^G: Goto  ^P: Config", " ^Q: Cancel",
        " ^Q: Cancel  ◀: back  ▶: Next", " ^Q: Cancel", " ^Q: Cancel"};
    int len = strlen(help_info[E.state]);
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | Ln: %d, Col: %d  ",
                        E.syntax ? E.syntax->file_type : "Plain Text", E.cy + 1,
                        E.rx + 1);
    if (len > cols)
        len = cols;

    abufAppendN(ab, help_info[E.state], len);

    while (len < cols) {
        if (cols - len == rlen) {
            abufAppendN(ab, rstatus, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
    abufAppend(ab, ANSI_CLEAR);
}

void editorDrawStatusMsgBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    abufAppend(ab, "\x1b[K");
    int len = strlen(E.status_msg);
    if (len > cols)
        len = cols;
    if (len)
        abufAppendN(ab, E.status_msg, len);

    abufAppend(ab, "\r\n");
}
