#include "status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "utils.h"
#include "version.h"

void editorSetStatusMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status_msg, sizeof(E.status_msg), fmt, ap);
    va_end(ap);
}

void editorDrawTopStatusBar(abuf* ab) {
    abufAppend(ab, "\x1b[48;5;234m");

    int cols = E.screen_cols;

    const char* title = "  nino v" EDITOR_VERSION " ";
    int title_len = strlen(title);

    char filename[255] = {0};
    int filename_len =
        snprintf(filename, sizeof(filename), "%s%s", E.dirty ? "*" : "",
                 E.filename ? E.filename : "Untitled");

    if (cols <= filename_len) {
        abufAppendN(ab, &filename[filename_len - cols], cols);
    } else {
        int center = (cols - filename_len) / 2;
        if (center > title_len) {
            abufAppendN(ab, title, title_len);
        } else {
            title_len = 0;
        }
        for (int i = title_len; i < cols; i++) {
            if (i == center) {
                abufAppendN(ab, filename, filename_len);
                i += filename_len;
            }
            abufAppend(ab, " ");
        }
    }

    abufAppend(ab, ANSI_CLEAR);
    abufAppend(ab, "\r\n");
}

void editorDrawStatusBar(abuf* ab) {
    int cols = E.cols + E.num_rows_digits + 1;
    char color[20];
    colorToANSI(E.color_cfg->status[0], color, 0);
    abufAppend(ab, color);
    colorToANSI(E.color_cfg->status[1], color, 1);
    abufAppend(ab, color);

    const char* help_str = "";
    const char* help_info[] = {
        " ^Q: Quit  ^S: Save  ^F: Find  ^G: Goto  ^P: Config",
        " ^Q: Cancel",
        " ^Q: Cancel  ◀: back  ▶: Next",
        " ^Q: Cancel",
        " ^Q: Cancel",
    };
    if (CONVAR_GETINT(helpinfo))
        help_str = help_info[E.state];
    int help_len = strlen(help_str);

    char rstatus[80];
    int rlen = snprintf(rstatus, sizeof(rstatus), "  %s | Ln: %d, Col: %d  ",
                        E.syntax ? E.syntax->file_type : "Plain Text",
                        E.cursor.y + 1, E.rx + 1);

    if (rlen > cols) {
        rlen = 0;
    }
    if (help_len + rlen > cols)
        help_len = cols - rlen;

    abufAppendN(ab, help_str, help_len);

    while (help_len < cols) {
        if (cols - help_len == rlen) {
            abufAppendN(ab, rstatus, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            help_len++;
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
