#include "prompt.h"

#include <ctype.h>
#include <stdarg.h>

#include "editor.h"
#include "input.h"
#include "output.h"
#include "prompt.h"
#include "row.h"
#include "terminal.h"
#include "unicode.h"
#include "utils.h"

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

static void editorSetRightPrompt(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gEditor.prompt_right, sizeof(gEditor.prompt_right), fmt, ap);
    va_end(ap);
}

static void promptRowNull(void) {
    EditorRow* row = &gEditor.prompt_row;
    editorRowEnsureCapacity(row, row->size + 1);
    row->data[row->size] = '\0';
}

// Returns start of the deleted range
static int promptDeleteSelect(int cx, int select_x) {
    EditorRow* row = &gEditor.prompt_row;
    int from = cx < select_x ? cx : select_x;
    int to = cx < select_x ? select_x : cx;
    editorRowDeleteRange(NULL, row, from, to);
    return from;
}

char* editorPrompt(const char* prefix,
                   int state,
                   void (*callback)(char*, int)) {
    int old_state = gEditor.state;
    gEditor.state = state;

    EditorRow* row = &gEditor.prompt_row;
    row->size = 0;

    // Store prefix; assume prefix is ASCII
    snprintf(gEditor.prompt_prefix, sizeof(gEditor.prompt_prefix), "%s",
             prefix);
    int prefix_rx = (int)strlen(gEditor.prompt_prefix);

    int cx = 0;
    bool is_selected = false;
    int select_x = 0;

    // Mouse state
    int64_t prev_click_time = 0;
    int mouse_click = 0;
    int prev_click_x = -1;
    bool mouse_pressed = false;

    while (true) {
        promptRowNull();
        gEditor.px = prefix_rx + editorRowCxToRx(row, cx);

        if (is_selected && cx != select_x) {
            int from = cx < select_x ? cx : select_x;
            int to = cx < select_x ? select_x : cx;
            gEditor.prompt_select_start_rx =
                prefix_rx + editorRowCxToRx(row, from);
            gEditor.prompt_select_end_rx = prefix_rx + editorRowCxToRx(row, to);
        } else {
            gEditor.prompt_select_start_rx = -1;
            gEditor.prompt_select_end_rx = -1;
        }

        editorRefreshScreen();

        EditorInput input = editorReadKey();
        int in_x = input.data.cursor.x;
        int in_y = input.data.cursor.y;

        switch (input.type) {
            case DEL_KEY:
                if (is_selected) {
                    cx = promptDeleteSelect(cx, select_x);
                    is_selected = false;
                } else if (cx < row->size) {
                    int next = editorRowNextUTF8(row, cx);
                    editorRowDeleteRange(NULL, row, cx, next);
                }
                if (callback) {
                    promptRowNull();
                    callback(row->data, input.type);
                }
                break;

            case CTRL_KEY('h'):
            case BACKSPACE:
                if (is_selected) {
                    cx = promptDeleteSelect(cx, select_x);
                    is_selected = false;
                } else if (cx > 0) {
                    int prev = editorRowPreviousUTF8(row, cx);
                    editorRowDeleteRange(NULL, row, prev, cx);
                    cx = prev;
                }
                if (callback) {
                    promptRowNull();
                    callback(row->data, input.type);
                }
                break;

            case PASTE_INPUT:
            case CTRL_KEY('v'): {
                EditorClipboard* clipboard = (input.type == PASTE_INPUT)
                                                 ? &input.data.paste
                                                 : &gEditor.clipboard;
                if (!clipboard->size)
                    break;
                // Only paste the first line
                const char* paste_buf = clipboard->lines[0].data;
                size_t paste_len = clipboard->lines[0].size;
                if (paste_len == 0)
                    break;

                if (is_selected) {
                    cx = promptDeleteSelect(cx, select_x);
                    is_selected = false;
                }
                editorRowInsertString(NULL, row, cx, paste_buf, paste_len);
                cx += (int)paste_len;

                if (callback) {
                    promptRowNull();
                    // send ctrl-v in case callback didn't handle PASTE_INPUT
                    callback(row->data, CTRL_KEY('v'));
                }
                break;
            }

            case SHIFT_HOME:
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = 0;
                break;

            case HOME_KEY:
                cx = 0;
                is_selected = false;
                break;

            case SHIFT_END:
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = row->size;
                break;

            case END_KEY:
                cx = row->size;
                is_selected = false;
                break;

            case SHIFT_LEFT:
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = editorRowPreviousUTF8(row, cx);
                break;

            case ARROW_LEFT:
                if (is_selected) {
                    cx = cx < select_x ? cx : select_x;
                    is_selected = false;
                } else {
                    cx = editorRowPreviousUTF8(row, cx);
                }
                break;

            case SHIFT_RIGHT:
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = editorRowNextUTF8(row, cx);
                break;

            case ARROW_RIGHT:
                if (is_selected) {
                    cx = cx > select_x ? cx : select_x;
                    is_selected = false;
                } else {
                    cx = editorRowNextUTF8(row, cx);
                }
                break;

            case SHIFT_CTRL_LEFT:
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = editorRowWordLeft(row, cx);
                break;

            case CTRL_LEFT:
                cx = editorRowWordLeft(row, cx);
                is_selected = false;
                break;

            case SHIFT_CTRL_RIGHT:
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = editorRowWordRight(row, cx);
                break;

            case CTRL_RIGHT:
                cx = editorRowWordRight(row, cx);
                is_selected = false;
                break;

            case CTRL_KEY('a'):
                if (row->size > 0) {
                    is_selected = true;
                    select_x = 0;
                    cx = row->size;
                }
                break;

            case CTRL_KEY('c'):
            case CTRL_KEY('x'): {
                if (!is_selected)
                    break;
                int from = cx < select_x ? cx : select_x;
                int to = cx < select_x ? select_x : cx;
                if (from == to)
                    break;

                editorFreeClipboardContent(&gEditor.clipboard);
                gEditor.clipboard.size = 1;
                gEditor.clipboard.lines = calloc_s(1, sizeof(Str));
                gEditor.clipboard.lines[0].data = malloc_s(to - from);
                memcpy(gEditor.clipboard.lines[0].data, &row->data[from],
                       to - from);
                gEditor.clipboard.lines[0].size = to - from;
                gEditor.copy_line = false;

                if (input.type == CTRL_KEY('x')) {
                    cx = promptDeleteSelect(cx, select_x);
                    is_selected = false;
                    if (callback) {
                        promptRowNull();
                        callback(row->data, input.type);
                    }
                }
            } break;

            case CTRL_KEY('d'):
                if (cx < row->size && !isIdentifierChar(row->data[cx]))
                    break;
                is_selected = true;
                editorRowSelectWord(row, cx, isNonIdentifierChar, &select_x,
                                    &cx);
                break;

            case WHEEL_UP:
            case WHEEL_DOWN: {
                int split_index;
                int field = editorGetMousePosField(in_x, in_y, &split_index);
                switch (field) {
                    case FIELD_TEXT:
                    case FIELD_LINENO:
                        editorScroll(split_index,
                                     (input.type == WHEEL_UP) ? -3 : 3);
                        break;
                    case FIELD_EXPLORER:
                        editorExplorerScroll((input.type == WHEEL_UP) ? -3 : 3);
                        break;
                    case FIELD_TOP_STATUS:
                        editorTopStatusBarScroll(split_index,
                                                 input.type == WHEEL_UP);
                        break;
                    default:
                        break;
                }
            } break;

            case MOUSE_PRESSED: {
                int field = editorGetMousePosField(in_x, in_y, NULL);
                if (field != FIELD_PROMPT) {
                    gEditor.state = old_state;
                    if (callback) {
                        promptRowNull();
                        // Send ESC so callback know we're quitting
                        callback(row->data, ESC);
                    }
                    row->size = 0;

                    gEditor.pending_input = input;
                    editorProcessKeypress();
                    return NULL;
                }

                // Click on prompt
                mouse_pressed = true;

                int64_t click_time = getTime();
                int64_t time_diff = click_time - prev_click_time;

                if (in_x == prev_click_x && time_diff / 1000 < 500) {
                    mouse_click++;
                } else {
                    mouse_click = 1;
                }
                prev_click_time = click_time;
                prev_click_x = in_x;

                int click_cx = 0;
                if (in_x >= prefix_rx) {
                    click_cx = editorRowRxToCx(row, in_x - prefix_rx);
                }

                switch (mouse_click % 3) {
                    case 1:
                        // Mouse to pos
                        cx = click_cx;
                        is_selected = false;
                        break;
                    case 2: {
                        // Select word
                        if (row->size == 0)
                            break;
                        int wcx = click_cx;
                        if (wcx >= row->size)
                            wcx = row->size > 0 ? row->size - 1 : 0;

                        IsCharFunc is_char;
                        if (isSpace(row->data[wcx])) {
                            is_char = isNonSpace;
                        } else if (isIdentifierChar(row->data[wcx])) {
                            is_char = isNonIdentifierChar;
                        } else {
                            is_char = isNonSeparator;
                        }
                        is_selected = true;
                        editorRowSelectWord(row, wcx, is_char, &select_x, &cx);
                    } break;
                    case 0:
                        // Select all
                        if (row->size > 0) {
                            is_selected = true;
                            select_x = 0;
                            cx = row->size;
                        }
                        break;
                }
            } break;

            case MOUSE_RELEASED:
                mouse_pressed = false;
                break;

            case MOUSE_MOVE: {
                if (!mouse_pressed)
                    break;
                int move_cx = 0;
                if (in_x >= prefix_rx) {
                    move_cx = editorRowRxToCx(row, in_x - prefix_rx);
                }
                if (!is_selected) {
                    is_selected = true;
                    select_x = cx;
                }
                cx = move_cx;
            } break;

            case CTRL_KEY('q'):
            case ESC:
                gEditor.state = old_state;
                if (callback) {
                    promptRowNull();
                    callback(row->data, input.type);
                }
                row->size = 0;
                return NULL;

            case '\r':
                if (row->size != 0) {
                    gEditor.state = old_state;
                    promptRowNull();
                    if (callback)
                        callback(row->data, input.type);
                    // Copy result
                    char* result = malloc_s(row->size + 1);
                    memcpy(result, row->data, row->size);
                    result[row->size] = '\0';
                    row->size = 0;
                    return result;
                }
                break;

            case CHAR_INPUT: {
                if (input.data.unicode != '\t') {
                    char output[4];
                    int len = encodeUTF8(input.data.unicode, output);
                    if (len == -1)
                        break;

                    if (is_selected) {
                        cx = promptDeleteSelect(cx, select_x);
                        is_selected = false;
                    }
                    editorRowInsertString(NULL, row, cx, output, len);
                    cx += len;
                }

                if (callback) {
                    promptRowNull();
                    callback(row->data, input.data.unicode);
                }
            } break;

            default:
                if (callback) {
                    promptRowNull();
                    callback(row->data, input.type);
                }
        }
        editorFreeInput(&input);
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

    EditorTab* tab = editorGetActiveTab();
    const EditorFile* file = editorTabGetFile(tab);

    int line = 0;
    strToInt(query, &line);  // If failed, 0 will still print the error.

    if (line < 0) {
        line = file->num_rows + 1 + line;
    }

    if (line > 0 && line <= file->num_rows) {
        tab->cursor.x = 0;
        tab->sx = 0;
        tab->cursor.y = line - 1;
        editorScrollToCursorCenter(gEditor.split_active_index);
    } else {
        editorMsg("Type a line number between 1 to %d (negative too).",
                  file->num_rows);
    }
}

void editorGotoLine(void) {
    char* query =
        editorPrompt("Goto line: ", STATE_GOTO_PROMPT, editorGotoCallback);
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

    EditorTab* tab = editorGetActiveTab();
    EditorFile* file = editorTabGetFile(tab);

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

        int ignorecase_mode = ignorecase.int_value;
        bool ignore_case = false;
        if (ignorecase_mode == 1) {
            ignore_case = true;
        } else if (ignorecase_mode == 2) {
            bool has_upper = false;
            for (size_t j = 0; j < len; j++) {
                if (isupper((unsigned char)query[j])) {
                    has_upper = true;
                    break;
                }
            }
            ignore_case = !has_upper;
        }

        FindList* cur = &head;
        for (int i = 0; i < file->num_rows; i++) {
            size_t col = 0;
            size_t row_len = (size_t)file->row[i].size;

            while (col < row_len) {
                int match_idx = findSubstring(file->row[i].data, row_len, query,
                                              len, col, ignore_case);
                if (match_idx < 0)
                    break;

                col = (size_t)match_idx;

                FindList* node = malloc_s(sizeof(FindList));
                node->prev = cur;
                node->next = NULL;
                node->row = i;
                node->col = (int)col;
                cur->next = node;
                cur = cur->next;
                tail_node = cur;

                total++;
                if (!match_node) {
                    current++;
                    if (((i == tab->cursor.y && col >= (size_t)tab->cursor.x) ||
                         i > tab->cursor.y)) {
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

        if (!match_node) {
            current = 1;
            match_node = head.next;
        }

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

    tab->cursor.x = match_node->col;
    tab->cursor.y = match_node->row;
    editorScrollToCursorCenter(gEditor.split_active_index);

    uint8_t* match_pos = &file->row[match_node->row].hl[match_node->col];
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
    char* query = editorPrompt("Find: ", STATE_FIND_PROMPT, editorFindCallback);
    if (query) {
        free(query);
    }
}
