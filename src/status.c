#include "status.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "highlight.h"
#include "unicode.h"
#include "utils.h"
#include "version.h"

void editorMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.con_msg[gEditor.con_rear], sizeof(gEditor.con_msg[0]),
              fmt, ap);
    va_end(ap);

    if (gEditor.con_front == gEditor.con_rear) {
        gEditor.con_front = (gEditor.con_front + 1) % EDITOR_CON_COUNT;
        gEditor.con_size--;
    } else if (gEditor.con_front == -1) {
        gEditor.con_front = 0;
    }
    gEditor.con_size++;
    gEditor.con_rear = (gEditor.con_rear + 1) % EDITOR_CON_COUNT;
}

void editorMsgClear(void) {
    gEditor.con_front = -1;
    gEditor.con_rear = 0;
    gEditor.con_size = 0;
}

void editorSetPrompt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.prompt, sizeof(gEditor.prompt), fmt, ap);
    va_end(ap);
}

void editorSetRightPrompt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.prompt_right, sizeof(gEditor.prompt_right), fmt, ap);
    va_end(ap);
}

void editorDrawTopStatusBar(abuf* ab) {
    const char* right_buf = "  nino v" EDITOR_VERSION " ";
    bool has_more_files = false;
    int rlen = strlen(right_buf);
    int len = gEditor.explorer.width;

    gotoXY(ab, 1, gEditor.explorer.width + 1);

    setColor(ab, gEditor.color_cfg.top_status[0], 0);
    setColor(ab, gEditor.color_cfg.top_status[1], 1);

    if (gEditor.tab_offset != 0) {
        abufAppendN(ab, "<", 1);
        len++;
    }

    gEditor.tab_displayed = 0;
    if (gEditor.loading) {
        const char* loading_text = "Loading...";
        int loading_text_len = strlen(loading_text);
        abufAppendN(ab, loading_text, loading_text_len);
        len = loading_text_len;
    } else {
        for (int i = 0; i < gEditor.file_count; i++) {
            if (i < gEditor.tab_offset)
                continue;

            const EditorFile* file = &gEditor.files[i];

            bool is_current = (file == gCurFile);
            if (is_current) {
                setColor(ab, gEditor.color_cfg.top_status[4], 0);
                setColor(ab, gEditor.color_cfg.top_status[5], 1);
            } else {
                setColor(ab, gEditor.color_cfg.top_status[2], 0);
                setColor(ab, gEditor.color_cfg.top_status[3], 1);
            }

            char buf[EDITOR_PATH_MAX] = {0};
            const char* filename =
                file->filename ? getBaseName(file->filename) : "Untitled";
            int buf_len = snprintf(buf, sizeof(buf), " %s%s ",
                                   file->dirty ? "*" : "", filename);
            int tab_width = strUTF8Width(buf);

            if (gEditor.screen_cols - len < tab_width ||
                (i != gEditor.file_count - 1 &&
                 gEditor.screen_cols - len == tab_width)) {
                has_more_files = true;
                if (gEditor.tab_displayed == 0) {
                    // Display at least one tab
                    // TODO: This is wrong
                    tab_width = gEditor.screen_cols - len - 1;
                    buf_len = gEditor.screen_cols - len - 1;
                } else {
                    break;
                }
            }

            // Not enough space to even show one tab
            if (tab_width < 0)
                break;

            abufAppendN(ab, buf, buf_len);
            len += tab_width;
            gEditor.tab_displayed++;
        }
    }

    setColor(ab, gEditor.color_cfg.top_status[0], 0);
    setColor(ab, gEditor.color_cfg.top_status[1], 1);

    if (has_more_files) {
        abufAppendN(ab, ">", 1);
        len++;
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
}

void editorDrawConMsg(abuf* ab) {
    if (gEditor.con_size == 0) {
        return;
    }

    setColor(ab, gEditor.color_cfg.prompt[0], 0);
    setColor(ab, gEditor.color_cfg.prompt[1], 1);

    bool should_draw_prompt =
        (gEditor.state != EDIT_MODE && gEditor.state != EXPLORER_MODE);
    int draw_x = gEditor.screen_rows - gEditor.con_size;
    if (should_draw_prompt) {
        draw_x--;
    }

    int index = gEditor.con_front;
    for (int i = 0; i < gEditor.con_size; i++) {
        gotoXY(ab, draw_x, 0);
        draw_x++;

        const char* buf = gEditor.con_msg[index];
        index = (index + 1) % EDITOR_CON_COUNT;

        int len = strlen(buf);
        if (len > gEditor.screen_cols) {
            len = gEditor.screen_cols;
        }

        abufAppendN(ab, buf, len);

        while (len < gEditor.screen_cols) {
            abufAppend(ab, " ");
            len++;
        }
    }
}

void editorDrawPrompt(abuf* ab) {
    bool should_draw_prompt =
        (gEditor.state != EDIT_MODE && gEditor.state != EXPLORER_MODE);
    if (!should_draw_prompt) {
        return;
    }

    setColor(ab, gEditor.color_cfg.prompt[0], 0);
    setColor(ab, gEditor.color_cfg.prompt[1], 1);

    gotoXY(ab, gEditor.screen_rows - 1, 0);

    const char* left = gEditor.prompt;
    int len = strlen(left);

    const char* right = gEditor.prompt_right;
    int rlen = strlen(right);

    if (rlen > gEditor.screen_cols) {
        rlen = 0;
    }

    if (len + rlen > gEditor.screen_cols) {
        len = gEditor.screen_cols - rlen;
    }

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
    gotoXY(ab, gEditor.screen_rows, 0);

    setColor(ab, gEditor.color_cfg.status[0], 0);
    setColor(ab, gEditor.color_cfg.status[1], 1);

    const char* help_str = "";
    const char* help_info[] = {
        " ^Q: Quit  ^O: Open  ^P: Prompt  ^S: Save  ^F: Find  ^G: Goto",
        " ^Q: Quit  ^O: Open  ^P: Prompt",
        " ^Q: Cancel  Up: back  Down: Next",
        " ^Q: Cancel",
        " ^Q: Cancel",
        " ^Q: Cancel",
        " ^Q: Cancel",
    };
    if (CONVAR_GETINT(helpinfo))
        help_str = help_info[gEditor.state];

    char lang[16];
    char pos[64];
    int len = strlen(help_str);
    int lang_len, pos_len;
    int rlen;
    if (gEditor.file_count == 0) {
        lang_len = 0;
        pos_len = 0;
    } else {
        const char* file_type =
            gCurFile->syntax ? gCurFile->syntax->file_type : "Plain Text";
        int row = gCurFile->cursor.y + 1;
        int col = editorRowCxToRx(&gCurFile->row[gCurFile->cursor.y],
                                  gCurFile->cursor.x) +
                  1;
        float line_percent = 0.0f;
        const char* nl_type = (gCurFile->newline == NL_UNIX) ? "LF" : "CRLF";
        if (gCurFile->num_rows - 1 > 0) {
            line_percent =
                (float)gCurFile->row_offset / (gCurFile->num_rows - 1) * 100.0f;
        }

        lang_len = snprintf(lang, sizeof(lang), "  %s  ", file_type);
        pos_len = snprintf(pos, sizeof(pos), " %d:%d [%.f%%] <%s> ", row, col,
                           line_percent, nl_type);
    }

    rlen = lang_len + pos_len;

    if (rlen > gEditor.screen_cols)
        rlen = 0;
    if (len + rlen > gEditor.screen_cols)
        len = gEditor.screen_cols - rlen;

    abufAppendN(ab, help_str, len);

    while (len < gEditor.screen_cols) {
        if (gEditor.screen_cols - len == rlen) {
            setColor(ab, gEditor.color_cfg.status[2], 0);
            setColor(ab, gEditor.color_cfg.status[3], 1);
            abufAppendN(ab, lang, lang_len);
            setColor(ab, gEditor.color_cfg.status[4], 0);
            setColor(ab, gEditor.color_cfg.status[5], 1);
            abufAppendN(ab, pos, pos_len);
            break;
        } else {
            abufAppend(ab, " ");
            len++;
        }
    }
}
