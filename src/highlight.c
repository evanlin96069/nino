#include "highlight.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"

void editorUpdateSyntax(EditorRow* row) {
    row->hl = realloc_s(row->hl, row->rsize);
    memset(row->hl, HL_NORMAL, row->rsize);
    memset(row->hl, 0, row->rsize);

    if (!CONVAR_GETINT(syntax) || !E.syntax)
        goto update_trailing;

    const char** keywords = E.syntax->keywords;

    const char* scs = E.syntax->singleline_comment_start;
    const char* mcs = E.syntax->multiline_comment_start;
    const char* mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = i > 0 ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&(row->render[i]), scs, scs_len)) {
                memset(&(row->hl[i]), HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_COMMENT;
                if (!strncmp(&(row->render[i]), mce, mce_len)) {
                    memset(&(row->hl[i]), HL_COMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                }
                i++;
                continue;
            } else if (!strncmp(&(row->render[i]), mcs, mcs_len)) {
                memset(&(row->hl[i]), HL_COMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string)
                    in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else if (c == '"' || c == '\'') {
                in_string = c;
                row->hl[i] = HL_STRING;
                i++;
                continue;
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                ((c == '.' || c == 'x' || c == 'X') && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j = 0;
            while (keywords[j]) {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
                int kw3 = keywords[j][klen - 1] == '^';
                if (kw2 || kw3)
                    klen--;
                int keyword_type = HL_KEYWORD1;
                if (kw2) {
                    keyword_type = HL_KEYWORD2;
                } else if (kw3) {
                    keyword_type = HL_KEYWORD3;
                }
                if (!strncmp(&(row->render[i]), keywords[j], klen) &&
                    isSeparator(row->render[i + klen])) {
                    memset(&(row->hl[i]), keyword_type, klen);
                    i += klen;
                    break;
                }
                j++;
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = isSeparator(c);
        i++;
    }
    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.num_rows)
        editorUpdateSyntax(&(E.row[row->idx + 1]));

update_trailing:
    if (CONVAR_GETINT(trailing)) {
        for (int i = row->rsize - 1; i >= 0; i--) {
            if (row->render[i] == ' ')
                row->hl[i] = HL_SPACE;
            else
                break;
        }
    }
}

void editorSelectSyntaxHighlight() {
    E.syntax = NULL;
    if (E.filename == NULL)
        return;

    char* ext = strrchr(E.filename, '.');

    for (unsigned int i = 0; i < HLDB_ENTRIES; i++) {
        const EditorSyntax* s = &HLDB[i];
        unsigned int j = 0;
        while (s->file_match[j]) {
            int is_ext = (s->file_match[j][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->file_match[j])) ||
                (!is_ext && strstr(E.filename, s->file_match[j]))) {
                E.syntax = s;

                for (int file_row = 0; file_row < E.num_rows; file_row++) {
                    editorUpdateSyntax(&(E.row[file_row]));
                }

                return;
            }
            j++;
        }
    }
}
