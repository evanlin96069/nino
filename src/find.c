#define _GNU_SOURCE

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "defines.h"
#include "editor.h"
#include "input.h"
#include "output.h"
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

    static unsigned char* saved_hl_pos = NULL;
    static unsigned char* saved_hl = NULL;
    static int saved_hl_len = 0;

    if (saved_hl && saved_hl_pos) {
        memcpy(saved_hl_pos, saved_hl, saved_hl_len);
        free(saved_hl);
        saved_hl = NULL;
        saved_hl_pos = NULL;
        saved_hl_len = 0;
    }

    // Quit find mode
    if (key == ESC || key == CTRL_KEY('q') || key == '\r') {
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
        return;
    }

    size_t len = strlen(query);
    if (len == 0)
        return;

    FindList* tail_node = NULL;
    if (!head.next || !prev_query || strcmp(prev_query, query) != 0) {
        // Recompute find list
        match_node = NULL;
        if (prev_query)
            free(prev_query);
        findListFree(head.next);
        head.next = NULL;

        prev_query = malloc_s(len + 1);
        memcpy(prev_query, query, len + 1);
        prev_query[len] = '\0';

        FindList* cur = &head;
        for (int i = 0; i < E.num_rows; i++) {
            char* match = NULL;
            int col = 0;
            char* (*search_func)(const char*, const char*) = &strstr;

            if (CONVAR_GETINT(ignorecase) == 1) {
                search_func = &strcasestr;
            } else if (CONVAR_GETINT(ignorecase) == 2) {
                bool has_upper = false;
                for (size_t i = 0; i < len; i++) {
                    if (isupper(query[i])) {
                        has_upper = true;
                        break;
                    }
                }
                if (!has_upper) {
                    search_func = &strcasestr;
                }
            }

            while ((match = (*search_func)(&E.row[i].data[col], query))) {
                col = match - E.row[i].data;
                FindList* node = malloc_s(sizeof(FindList));

                node->prev = cur;
                node->next = NULL;
                node->row = i;
                node->col = col;
                cur->next = node;
                cur = cur->next;

                tail_node = cur;

                if (!match_node &&
                    ((i == E.cursor.y && col >= E.cursor.x) || i > E.cursor.y))
                    match_node = cur;

                col += len;
            }
        }

        if (!head.next)
            return;

        if (!match_node)
            match_node = head.next;

        // Don't go back to head
        head.next->prev = tail_node;
    }

    if (!match_node)
        return;

    if (key == ARROW_DOWN) {
        if (match_node->next) {
            match_node = match_node->next;
        } else {
            match_node = head.next;
        }
    } else if (key == ARROW_UP) {
        match_node = match_node->prev;
    }

    E.cursor.x = match_node->col;
    E.cursor.y = match_node->row;
    editorScroll();
    E.row_offset = E.cursor.y - E.rows / 2;
    if (E.row_offset + E.rows > E.num_rows) {
        E.row_offset = E.num_rows - E.rows;
    }
    if (E.row_offset < 0) {
        E.row_offset = 0;
    }

    int rx = editorRowCxToRx(&E.row[match_node->row], match_node->col);
    unsigned char* match_pos = &(E.row[match_node->row].hl[rx]);
    saved_hl_len = len;
    saved_hl_pos = match_pos;
    saved_hl = malloc_s(len + 1);
    memcpy(saved_hl, match_pos, len);
    memset(match_pos, HL_MATCH, len);
}

void editorFind() {
    char* query = editorPrompt("Search: %s", FIND_MODE, editorFindCallback);
    if (query) {
        free(query);
    }
}
