#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "input.h"
#include "status.h"
#include "terminal.h"
#include "utils.h"

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
        editorSetRStatusMsg("");
        return;
    }

    size_t len = strlen(query);
    if (len == 0) {
        editorSetRStatusMsg("");
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
                for (size_t i = 0; i < len; i++) {
                    if (isupper(query[i])) {
                        has_upper = true;
                        break;
                    }
                }
                if (!has_upper) {
                    search_func = &strCaseStr;
                }
            }

            while (
                (match = (*search_func)(&gCurFile->row[i].data[col], query))) {
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
            editorSetRStatusMsg("  No results");
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
    editorSetRStatusMsg("  %d of %d", current, total);

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
