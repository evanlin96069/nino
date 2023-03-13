#include "highlight.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"

void editorUpdateSyntax(EditorFile* file, EditorRow* row) {
    row->hl = realloc_s(row->hl, row->size);
    memset(row->hl, HL_NORMAL, row->size);

    if (!CONVAR_GETINT(syntax) || !file->syntax)
        goto update_trailing;

    const char** keywords = file->syntax->keywords;

    const char* scs = file->syntax->singleline_comment_start;
    const char* mcs = file->syntax->multiline_comment_start;
    const char* mce = file->syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int row_index = (int)(row - file->row);
    int in_comment =
        (row_index > 0 && file->row[row_index - 1].hl_open_comment);

    int i = 0;
    while (i < row->size) {
        char c = row->data[i];
        unsigned char prev_hl = i > 0 ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->data[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_COMMENT;
                if (!strncmp(&row->data[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_COMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                }
                i++;
                continue;
            } else if (!strncmp(&row->data[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_COMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (file->syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->size) {
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

        if (file->syntax->flags & HL_HIGHLIGHT_NUMBERS) {
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
                if (!strncmp(&row->data[i], keywords[j], klen) &&
                    isSeparator(row->data[i + klen])) {
                    memset(&row->hl[i], keyword_type, klen);
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
    if (changed && row_index + 1 < file->num_rows)
        editorUpdateSyntax(file, &file->row[row_index + 1]);

update_trailing:
    if (CONVAR_GETINT(trailing)) {
        for (int i = row->size - 1; i >= 0; i--) {
            if (row->data[i] == ' ' || row->data[i] == '\t')
                row->hl[i] = HL_SPACE;
            else
                break;
        }
    }
}

void editorSelectSyntaxHighlight(EditorFile* file) {
    file->syntax = NULL;
    if (file->filename == NULL)
        return;

    char* ext = strrchr(file->filename, '.');

    for (unsigned int i = 0; i < HLDB_ENTRIES; i++) {
        const EditorSyntax* s = &HLDB[i];
        unsigned int j = 0;
        while (s->file_match[j]) {
            int is_ext = (s->file_match[j][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->file_match[j])) ||
                (!is_ext && strstr(file->filename, s->file_match[j]))) {
                file->syntax = s;

                for (int file_row = 0; file_row < file->num_rows; file_row++) {
                    editorUpdateSyntax(file, &file->row[file_row]);
                }

                return;
            }
            j++;
        }
    }
}
