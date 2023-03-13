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

    const char* right_buf = "  nino v" EDITOR_VERSION " ";
    int rlen = strlen(right_buf);

    int len = 0;
    if (gEditor.loading) {
        const char* loading_text = "Loading...";
        int loading_text_len = strlen(loading_text);
        abufAppendN(ab, loading_text, loading_text_len);
        len = loading_text_len;
    } else {
        for (int i = 0; i < gEditor.file_count; i++) {
            if (len >= gEditor.screen_cols)
                break;

            const EditorFile* file = &gEditor.files[i];

            bool is_current = (file == gCurFile);
            const char* effect = is_current ? ANSI_INVERT : ANSI_UNDERLINE;
            const char* not_effect =
                is_current ? ANSI_NOT_INVERT : ANSI_NOT_UNDERLINE;

            char buf[255] = {0};
            int buf_len =
                snprintf(buf, sizeof(buf), " %s%s ", file->dirty ? "*" : "",
                         file->filename ? file->filename : "Untitled");
            if (gEditor.screen_cols - len < buf_len) {
                buf_len = gEditor.screen_cols - len;
            }

            abufAppend(ab, effect);
            abufAppendN(ab, buf, buf_len);
            abufAppend(ab, not_effect);

            len += buf_len;
        }
    }

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            abufAppendN(ab, right_buf, rlen);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }

    abufAppend(ab, ANSI_CLEAR);
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

void editorDrawPrompt(abuf* ab) {
    char buf[32];

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", gEditor.screen_rows - 1, 0);
    abufAppend(ab, buf);

    colorToANSI(gEditor.color_cfg.prompt[0], buf, 0);
    abufAppend(ab, buf);
    colorToANSI(gEditor.color_cfg.prompt[1], buf, 1);
    abufAppend(ab, buf);

    drawLeftRightMsg(ab, gEditor.status_msg[0], gEditor.status_msg[1]);
}

void editorDrawStatusBar(abuf* ab) {
    char buf[32];

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", gEditor.screen_rows, 0);
    abufAppend(ab, buf);

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
