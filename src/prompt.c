#include "prompt.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "editor.h"
#include "input.h"
#include "output.h"
#include "prompt.h"
#include "terminal.h"
#include "unicode.h"

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

static void editorSetPrompt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.prompt, sizeof(gEditor.prompt), fmt, ap);
    va_end(ap);
}

static void editorSetRightPrompt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.prompt_right, sizeof(gEditor.prompt_right), fmt, ap);
    va_end(ap);
}

#define PROMPT_BUF_INIT_SIZE 64
#define PROMPT_BUF_GROWTH_RATE 2.0f

char* editorPrompt(char* prompt, int state, void (*callback)(char*, int)) {
    int old_state = gEditor.state;
    gEditor.state = state;

    size_t bufsize = PROMPT_BUF_INIT_SIZE;
    char* buf = malloc_s(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    int start = 0;
    while (prompt[start] != '\0' && prompt[start] != '%') {
        start++;
    }
    gEditor.px = start;
    while (true) {
        editorSetPrompt(prompt, buf);
        editorRefreshScreen();

        EditorInput input = editorReadKey();
        int x = input.data.cursor.x;
        int y = input.data.cursor.y;

        size_t idx = gEditor.px - start;
        switch (input.type) {
            case DEL_KEY:
                if (idx != buflen)
                    idx++;
                else
                    break;
                // fall through
            case CTRL_KEY('h'):
            case BACKSPACE:
                if (idx != 0) {
                    memmove(&buf[idx - 1], &buf[idx], buflen - idx + 1);
                    buflen--;
                    idx--;
                    if (callback)
                        callback(buf, input.type);
                }
                break;

            case CTRL_KEY('v'): {
                if (!gEditor.clipboard.size)
                    break;
                // Only paste the first line
                const char* paste_buf = gEditor.clipboard.data[0];
                size_t paste_len = strlen(paste_buf);
                if (paste_len == 0)
                    break;

                if (buflen + paste_len >= bufsize) {
                    bufsize = buflen + paste_len + 1;
                    bufsize *= PROMPT_BUF_GROWTH_RATE;
                    buf = realloc_s(buf, bufsize);
                }
                memmove(&buf[idx + paste_len], &buf[idx], buflen - idx + 1);
                memcpy(&buf[idx], paste_buf, paste_len);
                buflen += paste_len;
                idx += paste_len;

                if (callback)
                    callback(buf, input.type);

                break;
            }

            case HOME_KEY:
                idx = 0;
                break;

            case END_KEY:
                idx = buflen;
                break;

            case ARROW_LEFT:
                if (idx != 0)
                    idx--;
                break;

            case ARROW_RIGHT:
                if (idx < buflen)
                    idx++;
                break;

            case WHEEL_UP:
                editorScroll(-3);
                break;

            case WHEEL_DOWN:
                editorScroll(3);
                break;

            case MOUSE_PRESSED: {
                int field = getMousePosField(x, y);
                if (field == FIELD_PROMPT) {
                    if (x >= start) {
                        size_t cx = x - start;
                        if (cx < buflen)
                            idx = cx;
                        else
                            idx = buflen;
                    }
                    break;
                } else if (field == FIELD_TEXT) {
                    mousePosToEditorPos(&x, &y);
                    gCurFile->cursor.y = y;
                    gCurFile->cursor.x = editorRowRxToCx(&gCurFile->row[y], x);
                    gCurFile->sx = x;
                }
            }
            // fall through
            case CTRL_KEY('q'):
            case ESC:
                editorSetPrompt("");
                gEditor.state = old_state;
                if (callback)
                    callback(buf, input.type);
                free(buf);
                return NULL;

            case '\r':
                if (buflen != 0) {
                    editorSetPrompt("");
                    gEditor.state = old_state;
                    if (callback)
                        callback(buf, input.type);
                    return buf;
                }
                break;

            case CHAR_INPUT: {
                char output[4];
                int len = encodeUTF8(input.data.unicode, output);
                if (len == -1)
                    return buf;

                if (buflen + len >= bufsize) {
                    bufsize += len;
                    bufsize *= PROMPT_BUF_GROWTH_RATE;
                    buf = realloc_s(buf, bufsize);
                }
                memmove(&buf[idx + len], &buf[idx], buflen - idx + 1);
                memcpy(&buf[idx], output, len);
                buflen += len;
                idx += len;

                // TODO: Support Unicode characters in prompt

                if (callback)
                    callback(buf, input.data.unicode);
            } break;

            default:
                if (callback)
                    callback(buf, input.type);
        }
        gEditor.px = start + idx;
    }
}

// Goto

static void editorGotoCallback(char* query, int key) {
    if (key == ESC || key == CTRL_KEY('q')) {
        return;
    }

    editorMsgClear();

    if (query == NULL || query[0] == '\0') {
        return;
    }

    int line = strToInt(query);

    if (line < 0) {
        line = gCurFile->num_rows + 1 + line;
    }

    if (line > 0 && line <= gCurFile->num_rows) {
        gCurFile->cursor.x = 0;
        gCurFile->sx = 0;
        gCurFile->cursor.y = line - 1;
        editorScrollToCursorCenter();
    } else {
        editorMsg("Type a line number between 1 to %d (negative too).",
                  gCurFile->num_rows);
    }
}

void editorGotoLine(void) {
    char* query =
        editorPrompt("Goto line: %s", GOTO_LINE_MODE, editorGotoCallback);
    if (query) {
        free(query);
    }
}

// Find

typedef struct FindList {
    struct FindList* prev;
    struct FindList* next;
    int row;
    int col;
} FindList;

static void findListFree(FindList* thisptr) {
    FindList* temp;
    while (thisptr) {
        temp = thisptr;
        thisptr = thisptr->next;
        free(temp);
    }
}

static void editorFindCallback(char* query, int key) {
    static char* prev_query = NULL;
    static FindList head = {.prev = NULL, .next = NULL};
    static FindList* match_node = NULL;

    static uint8_t* saved_hl_pos = NULL;
    static uint8_t* saved_hl = NULL;
    static size_t saved_hl_len = 0;

    static int total = 0;
    static int current = 0;

    if (saved_hl && saved_hl_pos) {
        memcpy(saved_hl_pos, saved_hl, saved_hl_len);
        free(saved_hl);
        saved_hl = NULL;
        saved_hl_pos = NULL;
        saved_hl_len = 0;
    }

    // Quit find mode
    if (key == ESC || key == CTRL_KEY('q') || key == '\r' ||
        key == MOUSE_PRESSED) {
        if (prev_query) {
            free(prev_query);
            prev_query = NULL;
        }
        if (saved_hl) {
            free(saved_hl);
            saved_hl = NULL;
        }
        findListFree(head.next);
        head.next = NULL;
        editorSetRightPrompt("");
        return;
    }

    size_t len = strlen(query);
    if (len == 0) {
        editorSetRightPrompt("");
        return;
    }

    FindList* tail_node = NULL;
    if (!head.next || !prev_query || strcmp(prev_query, query) != 0) {
        // Recompute find list

        total = 0;
        current = 0;

        match_node = NULL;
        if (prev_query)
            free(prev_query);
        findListFree(head.next);
        head.next = NULL;

        prev_query = malloc_s(len + 1);
        memcpy(prev_query, query, len + 1);
        prev_query[len] = '\0';

        FindList* cur = &head;
        for (int i = 0; i < gCurFile->num_rows; i++) {
            char* match = NULL;
            int col = 0;
            char* (*search_func)(const char*, const char*) = &strstr;

            if (CONVAR_GETINT(ignorecase) == 1) {
                search_func = &strCaseStr;
            } else if (CONVAR_GETINT(ignorecase) == 2) {
                bool has_upper = false;
                for (size_t j = 0; j < len; j++) {
                    if (isupper((uint8_t)query[j])) {
                        has_upper = true;
                        break;
                    }
                }
                if (!has_upper) {
                    search_func = &strCaseStr;
                }
            }
            while (col + len < (uint32_t)gCurFile->row[i].size &&
                   (match = (*search_func)(&gCurFile->row[i].data[col],
                                           query)) != 0) {
                col = match - gCurFile->row[i].data;
                FindList* node = malloc_s(sizeof(FindList));

                node->prev = cur;
                node->next = NULL;
                node->row = i;
                node->col = col;
                cur->next = node;
                cur = cur->next;
                tail_node = cur;

                total++;
                if (!match_node) {
                    current++;
                    if (((i == gCurFile->cursor.y &&
                          col >= gCurFile->cursor.x) ||
                         i > gCurFile->cursor.y)) {
                        match_node = cur;
                    }
                }
                col += len;
            }
        }

        if (!head.next) {
            editorSetRightPrompt("  No results");
            return;
        }

        if (!match_node)
            match_node = head.next;

        // Don't go back to head
        head.next->prev = tail_node;
    }

    if (key == ARROW_DOWN) {
        if (match_node->next) {
            match_node = match_node->next;
            current++;
        } else {
            match_node = head.next;
            current = 1;
        }
    } else if (key == ARROW_UP) {
        match_node = match_node->prev;
        if (current == 1)
            current = total;
        else
            current--;
    }
    editorSetRightPrompt("  %d of %d", current, total);

    gCurFile->cursor.x = match_node->col;
    gCurFile->cursor.y = match_node->row;

    editorScrollToCursorCenter();

    uint8_t* match_pos = &gCurFile->row[match_node->row].hl[match_node->col];
    saved_hl_len = len;
    saved_hl_pos = match_pos;
    saved_hl = malloc_s(len + 1);
    memcpy(saved_hl, match_pos, len);
    for (size_t i = 0; i < len; i++) {
        match_pos[i] &= ~HL_BG_MASK;
        match_pos[i] |= HL_BG_MATCH << HL_FG_BITS;
    }
}

void editorFind(void) {
    char* query = editorPrompt("Find: %s", FIND_MODE, editorFindCallback);
    if (query) {
        free(query);
    }
}
