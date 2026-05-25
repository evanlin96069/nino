#include "prompt.h"

#include <stdarg.h>

#include "config.h"
#include "editor.h"
#include "input.h"
#include "output.h"
#include "prompt.h"
#include "row.h"
#include "terminal.h"
#include "unicode.h"
#include "utils.h"

static void editorVMsg(const char* fmt, va_list ap) {
    vsnprintf(gEditor.con_msg[gEditor.con_rear], sizeof(gEditor.con_msg[0]),
              fmt, ap);

    if (gEditor.con_front == gEditor.con_rear) {
        gEditor.con_front = (gEditor.con_front + 1) % EDITOR_CON_COUNT;
        gEditor.con_size--;
    } else if (gEditor.con_front == -1) {
        gEditor.con_front = 0;
    }
    gEditor.con_size++;
    gEditor.con_rear = (gEditor.con_rear + 1) % EDITOR_CON_COUNT;
}

void editorMsg(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    editorVMsg(fmt, ap);
    va_end(ap);
}

void editorDevMsg(const char* fmt, ...) {
    if (!developer.int_value)
        return;

    va_list ap;
    va_start(ap, fmt);
    editorVMsg(fmt, ap);
    va_end(ap);
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

                int64_t click_time = input.timestamp_ms;
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

// Well, the actual size is FIND_MAX_HISTORY - 1
// for easy circular queue implementation
#define FIND_MAX_HISTORY 32

typedef struct FindPos {
    int row, col;
} FindPos;

typedef VECTOR(FindPos) VecFindPos;

typedef struct FindCacheEntry {
    char* query;
    size_t query_len;
    bool ignore_case;
    VecFindPos matches;
} FindCacheEntry;

typedef struct FindState {
    bool ignore_case;
    int index;  // index into cache array
    int match;  // index into FindCacheEntry matches

    // Cache (circular queue)
    int head, tail;
    FindCacheEntry cache[FIND_MAX_HISTORY];
} FindState;

static void findCacheFree(FindCacheEntry* entry) {
    free(entry->query);
    vector_free(entry->matches);
}

static inline bool findStateIsEmpty(FindState* state) {
    return state->head == -1;
}

static inline bool findStateIsFull(FindState* state) {
    return ((state->tail + 1) % FIND_MAX_HISTORY) == state->head;
}

static void findStateFree(FindState* state) {
    if (findStateIsEmpty(state))
        return;

    for (int i = state->head; i != state->tail;
         i = (i + 1) % FIND_MAX_HISTORY) {
        findCacheFree(&state->cache[i]);
    }

    state->index = -1;
    state->match = -1;
    state->head = -1;
    state->tail = -1;
}

static void findStatePop(FindState* state) {
    if (findStateIsEmpty(state))
        return;

    findCacheFree(&state->cache[state->head]);
    state->head = (state->head + 1) % FIND_MAX_HISTORY;

    if (state->head == state->tail) {
        // Queue is now empty
        state->head = -1;
        state->tail = -1;
    }
}

static int findStateAppend(FindState* state, FindCacheEntry* cache) {
    if (findStateIsFull(state)) {
        findStatePop(state);
    }

    if (findStateIsEmpty(state)) {
        state->head = 0;
        state->tail = 0;
    }

    state->cache[state->tail] = *cache;
    int index = state->tail;
    state->tail = (state->tail + 1) % FIND_MAX_HISTORY;
    return index;
}

// Returns the best matched cache, -1 if no match
static int findStateSearchCache(FindState* state, const char* new_query) {
    if (findStateIsEmpty(state))
        return -1;

    int best_index = -1;
    size_t best_len = 0;
    bool best_exact_mode = false;

    size_t new_query_len = strlen(new_query);

    for (int i = state->head; i != state->tail;
         i = (i + 1) % FIND_MAX_HISTORY) {
        // Ignore-case cannot use case-sensitive cache
        if (state->ignore_case && !state->cache[i].ignore_case)
            continue;
        if (state->cache[i].query_len > new_query_len)
            continue;

        bool exact_mode = (state->cache[i].ignore_case == state->ignore_case);

        if (best_index != -1) {
            // Prefer same-mode over cross-mode
            if (best_exact_mode && !exact_mode)
                continue;
            // Prefer longer prefix match
            if (best_exact_mode == exact_mode &&
                best_len >= state->cache[i].query_len)
                continue;
        }

        if (strStartsWith(new_query, state->cache[i].query,
                          state->cache[i].ignore_case)) {
            best_index = i;
            best_len = state->cache[i].query_len;
            best_exact_mode = exact_mode;
        }
    }

    return best_index;
}

static void editorFindCallback(char* query, int key) {
    static FindState state = {
        .index = -1,
        .match = -1,
        .head = -1,
        .tail = -1,
    };

    EditorTab* tab = editorGetActiveTab();
    EditorFile* file = editorTabGetFile(tab);

    tab->has_match = false;

    // Quit find mode
    if (key == ESC || key == CTRL_KEY('q') || key == '\r' ||
        key == MOUSE_PRESSED) {
        findStateFree(&state);
        editorSetRightPrompt("");
        return;
    }

    size_t query_len = strlen(query);
    if (query_len == 0) {
        editorSetRightPrompt("");
        return;
    }

    if (state.index == -1 ||
        (query_len != state.cache[state.index].query_len ||
         strcmp(state.cache[state.index].query, query) != 0)) {
        // Query changed
        state.index = -1;
        state.match = -1;

        // Case
        int ignorecase_mode = ignorecase.int_value;
        bool ignore_case = false;
        if (ignorecase_mode == 1) {
            ignore_case = true;
        } else if (ignorecase_mode == 2) {
            bool has_upper = false;
            for (size_t j = 0; j < query_len; j++) {
                if (isUpper(query[j])) {
                    has_upper = true;
                    break;
                }
            }
            ignore_case = !has_upper;
        }

        // Search the cache
        state.ignore_case = ignore_case;
        int cache_index = findStateSearchCache(&state, query);

        if (cache_index == -1 ||
            (state.cache[cache_index].query_len != query_len &&
             state.cache[cache_index].matches.size != 0)) {
            FindCacheEntry cache = {
                .query_len = query_len,
                .ignore_case = ignore_case,
            };

            size_t query_size = (query_len + 1) * sizeof(char);
            cache.query = malloc_s(query_size);
            memcpy(cache.query, query, query_size);

            if (cache_index == -1) {
                // Search all matches
                editorDevMsg("Find: No cache hit, full search");
                for (int i = 0; i < file->num_rows; i++) {
                    size_t col = 0;
                    size_t row_len = (size_t)file->row[i].size;

                    while (col < row_len) {
                        int match_idx =
                            findSubstring(file->row[i].data, row_len, query,
                                          query_len, col, ignore_case);
                        if (match_idx < 0)
                            break;

                        col = (size_t)match_idx;
                        vector_push(cache.matches, (FindPos){
                                                       .row = i,
                                                       .col = col,
                                                   });
                        col += query_len;
                    }
                }
            } else {
                // Search in the matched cache
                const VecFindPos* matches = &state.cache[cache_index].matches;
                size_t prefix_len = state.cache[cache_index].query_len;
                bool cross_mode =
                    (state.cache[cache_index].ignore_case != ignore_case);

                if (cross_mode) {
                    // Cross-mode: has to verify the entire query
                    editorDevMsg("Find: Cache hit, cross-mode prefix matched");
                    for (size_t i = 0; i < matches->size; i++) {
                        FindPos match = matches->data[i];
                        if ((size_t)match.col + query_len >
                            (size_t)file->row[match.row].size)
                            continue;
                        // We checked the size, this should be safe
                        if (!strStartsWith(
                                file->row[match.row].data + match.col, query,
                                ignore_case))
                            continue;
                        vector_push(cache.matches, matches->data[i]);
                    }
                } else {
                    // Same-mode: only verify the suffix after the prefix
                    editorDevMsg("Find: Cache hit, prefix matched");
                    size_t search_len = query_len - prefix_len;
                    const char* search_start = query + prefix_len;
                    for (size_t i = 0; i < matches->size; i++) {
                        FindPos match = matches->data[i];
                        match.col += prefix_len;
                        if ((size_t)match.col + search_len >
                            (size_t)file->row[match.row].size)
                            continue;
                        // We checked the size, this should be safe
                        if (!strStartsWith(
                                file->row[match.row].data + match.col,
                                search_start, ignore_case))
                            continue;
                        // Push the original pos
                        vector_push(cache.matches, matches->data[i]);
                    }
                }
            }
            state.index = findStateAppend(&state, &cache);
        } else {
            // Exact match or matched a no result entry
            if (state.cache[cache_index].matches.size == 0) {
                editorDevMsg("Find: Cache hit, empty result");
            } else {
                editorDevMsg("Find: Cache hit, exact match");
            }
            state.index = cache_index;
        }
    }

    if (state.index == -1 || state.cache[state.index].matches.size == 0) {
        editorSetRightPrompt("  No results");
        return;
    }

    const VecFindPos* matches = &state.cache[state.index].matches;
    if (state.match == -1) {
        // Find the next match in the current matches
        state.match = 0;  // If not found later, jump to the start
        for (size_t i = 0; i < matches->size; i++) {
            FindPos match = matches->data[i];
            if (match.row > tab->cursor.y ||
                (match.row == tab->cursor.y && match.col >= tab->cursor.x)) {
                state.match = i;
                break;
            }
        }
    }

    if (key == ARROW_DOWN) {
        state.match = (state.match + 1) % matches->size;
    } else if (key == ARROW_UP) {
        state.match = (state.match + matches->size - 1) % matches->size;
    }
    editorSetRightPrompt("  %d of %d", state.match + 1, matches->size);

    int match_col = matches->data[state.match].col;
    int match_row = matches->data[state.match].row;
    tab->cursor.x = match_col;
    tab->cursor.y = match_row;
    editorScrollToCursorCenter(gEditor.split_active_index);

    tab->has_match = true;
    tab->match_row = match_row;
    tab->match_col = match_col;
    tab->match_len = query_len;
}

void editorFind(void) {
    char* query = editorPrompt("Find: ", STATE_FIND_PROMPT, editorFindCallback);
    if (query) {
        free(query);
    }
}
