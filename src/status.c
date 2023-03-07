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
    vsnprintf(gEditor.status_msg[0], sizeof(gEditor.status_msg[0]), fmt, ap);
    va_end(ap);
}

void editorSetRStatusMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.status_msg[1], sizeof(gEditor.status_msg[1]), fmt, ap);
    va_end(ap);
}

void editorDrawTopStatusBar(abuf* ab) {
    char buf[32];
    colorToANSI(gEditor.color_cfg.top_status[0], buf, 0);
    abufAppend(ab, buf);
    colorToANSI(gEditor.color_cfg.top_status[1], buf, 1);
    abufAppend(ab, buf);

    int cols = gEditor.screen_cols;

    const char* title = "  nino v" EDITOR_VERSION " ";
    int title_len = strlen(title);

    char filename[255] = {0};
    int filename_len = snprintf(
        filename, sizeof(filename), "%s%s", gCurFile->dirty ? "*" : "",
        gCurFile->filename ? gCurFile->filename
                           : (gEditor.loading ? "Loading..." : "Untitled"));

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

static void drawLeftRightMsg(abuf* ab, const char* left, const char* right) {
    int len = strlen(left);
    int rlen = strlen(right);

    if (rlen > gEditor.screen_cols)
        rlen = 0;
    if (len + rlen > gEditor.screen_cols)
        len = gEditor.screen_cols - rlen;

    abufAppendN(ab, left, len);

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            abufAppendN(ab, right, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}

void editorDrawStatusBar(abuf* ab) {
    char buf[32];
    colorToANSI(gEditor.color_cfg.status[0], buf, 0);
    abufAppend(ab, buf);
    colorToANSI(gEditor.color_cfg.status[1], buf, 1);
    abufAppend(ab, buf);

    const char* help_str = "";
    const char* help_info[] = {
        " ^Q: Quit  ^S: Save  ^F: Find  ^G: Goto  ^P: Config",
        " ^Q: Cancel",
        " ^Q: Cancel  Up: back  Down: Next",
        " ^Q: Cancel",
        " ^Q: Cancel",
    };
    if (CONVAR_GETINT(helpinfo))
        help_str = help_info[gEditor.state];

    char rstatus[64];
    snprintf(rstatus, sizeof(rstatus), "  %s | Ln: %d, Col: %d  ",
             gCurFile->syntax ? gCurFile->syntax->file_type : "Plain Text",
             gCurFile->cursor.y + 1,
             editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                             gCurFile->cursor.x) +
                 1);

    drawLeftRightMsg(ab, help_str, rstatus);
    abufAppend(ab, ANSI_CLEAR);
}

void editorDrawStatusMsgBar(abuf* ab) {
    char buf[32];

    colorToANSI(gEditor.color_cfg.prompt[0], buf, 0);
    abufAppend(ab, buf);
    colorToANSI(gEditor.color_cfg.prompt[1], buf, 1);
    abufAppend(ab, buf);

    drawLeftRightMsg(ab, gEditor.status_msg[0], gEditor.status_msg[1]);
    abufAppend(ab, "\r\n");
}
