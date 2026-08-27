#include "search.h"

#include "console.h"
#include "editor.h"

#include "panels/edit.h"
#include "panels/prompt.h"

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

static void findCallback(PromptEvent event, void* user_data) {
    FindState* state = (FindState*)user_data;

    EditorTab* tab = editorGetActiveTab();
    const EditorFile* file = editorTabGetFile(tab);
    tab->has_match = false;

    if (event.type == PROMPT_EVENT_SUBMIT ||
        event.type == PROMPT_EVENT_CANCEL) {
        findStateFree(state);
        editorSetRightPrompt("");
        editorHelpRestoreMsg();
        return;
    }

    const char* query = event.query;
    size_t query_len = strlen(query);
    if (query_len == 0) {
        editorSetRightPrompt("");
        return;
    }

    if (state->index == -1 ||
        (query_len != state->cache[state->index].query_len ||
         strcmp(state->cache[state->index].query, query) != 0)) {
        // Query changed
        state->index = -1;
        state->match = -1;

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
        state->ignore_case = ignore_case;
        int cache_index = findStateSearchCache(state, query);

        if (cache_index == -1 ||
            (state->cache[cache_index].matches.size != 0 &&
             (state->cache[cache_index].ignore_case != ignore_case ||
              state->cache[cache_index].query_len != query_len))) {
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
                const VecFindPos* matches = &state->cache[cache_index].matches;
                size_t prefix_len = state->cache[cache_index].query_len;
                bool cross_mode =
                    (state->cache[cache_index].ignore_case != ignore_case);

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
            state->index = findStateAppend(state, &cache);
        } else {
            // Exact match or matched a no result entry
            if (state->cache[cache_index].matches.size == 0) {
                editorDevMsg("Find: Cache hit, empty result");
            } else {
                editorDevMsg("Find: Cache hit, exact match");
            }
            state->index = cache_index;
        }
    }

    if (state->index == -1 || state->cache[state->index].matches.size == 0) {
        editorSetRightPrompt("  No results");
        return;
    }

    const VecFindPos* matches = &state->cache[state->index].matches;
    if (state->match == -1) {
        // Find the next match in the current matches
        state->match = 0;  // If not found later, jump to the start
        for (size_t i = 0; i < matches->size; i++) {
            FindPos match = matches->data[i];
            if (match.row > tab->cursor.y ||
                (match.row == tab->cursor.y && match.col >= tab->cursor.x)) {
                state->match = i;
                break;
            }
        }
    }

    if (event.type == PROMPT_EVENT_KEY) {
        if (event.key_event.value == KEY_EVENT(KEY_DOWN)) {
            state->match = (state->match + 1) % matches->size;
        } else if (event.key_event.value == KEY_EVENT(KEY_UP)) {
            state->match = (state->match + matches->size - 1) % matches->size;
        }
    }

    editorSetRightPrompt("  %d of %d", state->match + 1, matches->size);

    int match_col = matches->data[state->match].col;
    int match_row = matches->data[state->match].row;
    tab->cursor.x = match_col;
    tab->cursor.y = match_row;
    tab->cursor.is_selected = false;
    tab->cursor.select_x = tab->cursor.x;
    tab->cursor.select_y = tab->cursor.y;
    editorScrollToCursorCenter(gEditor.active_edit_panel);

    tab->has_match = true;
    tab->match_row = match_row;
    tab->match_col = match_col;
    tab->match_len = query_len;
}

void editorPromptFind(void) {
    static FindState find_state;

    find_state.index = -1;
    find_state.match = -1;
    find_state.head = -1;
    find_state.tail = -1;

    editorHelpSetMsg(HELP_FIND_PROMPT);
    editorPrompt("Find: ", findCallback, &find_state);
}

// Goto

static void gotoCallback(PromptEvent event, void* user_data) {
    UNUSED(user_data);

    if (event.type == PROMPT_EVENT_SUBMIT ||
        event.type == PROMPT_EVENT_CANCEL) {
        editorHelpRestoreMsg();
        return;
    }

    editorMsgClear();

    const char* query = event.query;
    if (query == NULL || query[0] == '\0') {
        return;
    }

    EditPanel* split = gEditor.active_edit_panel;
    if (!split) {
        return;
    }

    EditorTab* tab = editorSplitGetTab(split);
    const EditorFile* file = editorTabGetFile(tab);

    int line = 0;
    strToInt(query, &line);  // If failed, 0 will still print the error.

    if (line < 0) {
        line = file->num_rows + 1 + line;
    }

    if (line > 0 && line <= file->num_rows) {
        tab->sx = 0;
        tab->cursor.x = 0;
        tab->cursor.y = line - 1;
        tab->cursor.is_selected = false;
        tab->cursor.select_x = tab->cursor.x;
        tab->cursor.select_y = tab->cursor.y;
        editorScrollToCursorCenter(split);
    } else {
        editorMsg("Type a line number between 1 to %d (negative too).",
                  file->num_rows);
    }
}

void editorPromptGoto(void) {
    editorHelpSetMsg(HELP_GOTO_PROMPT);
    editorPrompt("Goto line: ", gotoCallback, NULL);
}
