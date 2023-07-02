#include "highlight.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dirent.h>
#endif

#include "config.h"

void editorUpdateSyntax(EditorFile* file, EditorRow* row) {
    row->hl = realloc_s(row->hl, row->size);
    memset(row->hl, HL_NORMAL, row->size);

    EditorSyntax* s = file->syntax;

    if (!CONVAR_GETINT(syntax) || !s)
        goto update_trailing;

    const char* scs = s->singleline_comment_start;
    const char* mcs = s->multiline_comment_start;
    const char* mce = s->multiline_comment_end;

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

        if (scs_len && !in_string && !in_comment) {
            if (strncmp(&row->data[i], scs, scs_len) == 0) {
                memset(&row->hl[i], HL_COMMENT, row->size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_COMMENT;
                if (strncmp(&row->data[i], mce, mce_len) == 0) {
                    memset(&row->hl[i], HL_COMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                }
                i++;
                continue;
            } else if (strncmp(&row->data[i], mcs, mcs_len) == 0) {
                memset(&row->hl[i], HL_COMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        if (s->flags & HL_HIGHLIGHT_STRINGS) {
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

        if (s->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) || c == '.') && prev_sep) {
                int start = i;
                i++;
                if (c == '0') {
                    if (row->data[i] == 'x' || row->data[i] == 'X') {
                        // hex
                        i++;
                        while (isdigit(row->data[i]) ||
                               (row->data[i] >= 'a' && row->data[i] <= 'f') ||
                               (row->data[i] >= 'A' && row->data[i] <= 'F')) {
                            i++;
                        }
                    } else if (row->data[i] >= '0' && row->data[i] <= '7') {
                        // oct
                        i++;
                        while (row->data[i] >= '0' && row->data[i] <= '7') {
                            i++;
                        }
                    } else if (row->data[i] == '.') {
                        // float
                        i++;
                        while (isdigit(row->data[i])) {
                            i++;
                        }
                    }
                } else {
                    while (isdigit(row->data[i])) {
                        i++;
                    }
                    if (c != '.' && row->data[i] == '.') {
                        i++;
                        while (isdigit(row->data[i])) {
                            i++;
                        }
                    }
                }
                if (c == '.' && i - start == 1)
                    continue;

                if (row->data[i] == 'f' || row->data[i] == 'F')
                    i++;
                if (isSeparator(row->data[i]) || isspace(row->data[i]))
                    memset(&row->hl[start], HL_NUMBER, i - start);
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            bool found_keyword = false;
            for (int kw = 0; kw < 3; kw++) {
                for (size_t j = 0; j < s->keywords[kw].size; j++) {
                    int klen = strlen(s->keywords[kw].data[j]);
                    int keyword_type = HL_KEYWORD1 + kw;
                    if (strncmp(&row->data[i], s->keywords[kw].data[j], klen) ==
                            0 &&
                        isNonIdentifierChar(row->data[i + klen])) {
                        found_keyword = true;
                        memset(&row->hl[i], keyword_type, klen);
                        i += klen;
                        break;
                    }
                }
                if (found_keyword) {
                    break;
                }
            }

            if (found_keyword) {
                prev_sep = 0;
                continue;
            }
        }
        prev_sep = isNonIdentifierChar(c);
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

    for (EditorSyntax* s = gEditor.HLDB; s; s = s->next) {
        for (size_t i = 0; i < s->file_exts.size; i++) {
            int is_ext = (s->file_exts.data[i][0] == '.');
            if ((is_ext && ext && strcmp(ext, s->file_exts.data[i]) == 0) ||
                (!is_ext && strstr(file->filename, s->file_exts.data[i]))) {
                file->syntax = s;
                for (int file_row = 0; file_row < file->num_rows; file_row++) {
                    editorUpdateSyntax(file, &file->row[file_row]);
                }
                return;
            }
        }
    }
}

void editorLoadDefaultHLDB(void) {
    char path[PATH_MAX];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s\\.nino\\syntax", getenv("HOME"));

    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile(path, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char file_path[PATH_MAX];
            snprintf(file_path, PATH_MAX, "%s\\%s", path, findData.cFileName);
            editorLoadHLDB(file_path);
        }
    } while (FindNextFile(hFind, &findData) != 0);

    FindClose(hFind);
#else
    snprintf(path, sizeof(path), "%s/.config/nino/syntax", getenv("HOME"));

    DIR* directory = opendir(path);
    if (!directory)
        return;

    struct dirent* entry;
    while ((entry = readdir(directory)) != NULL) {
        if (entry->d_type == DT_REG) {
            const char* ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".json") == 0) {
                char file_path[PATH_MAX];
                snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
                editorLoadHLDB(file_path);
            }
        }
    }
    closedir(directory);
#endif
}

#define ARENA_SIZE (1 << 17)
static EditorSyntax* HLDB_tail = NULL;
bool editorLoadHLDB(const char* json_file) {
    FILE* fp;
    size_t size;
    char* buffer;

    // Load file
    fp = fopen(json_file, "r");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    rewind(fp);

    buffer = calloc_s(1, size + 1);

    if (fread(buffer, size, 1, fp) != 1) {
        fclose(fp);
        free(buffer);
        return false;
    }
    fclose(fp);

    // Parse json
    Arena arena;
    arenaInit(&arena, ARENA_SIZE);
    JsonValue* value = jsonParse(buffer, &arena);
    free(buffer);
    if (value->type != JSON_OBJECT) {
        arenaDeinit(&arena);
        return false;
    }

    // Get data
#define CHECK(boolean)  \
    do {                \
        if (!(boolean)) \
            goto END;   \
    } while (0)

    EditorSyntax* syntax = calloc_s(1, sizeof(EditorSyntax));
    JsonObject* object = value->object;

    JsonValue* name = jsonObjectFind(object, "name");
    CHECK(name && name->type == JSON_STRING);
    syntax->file_type = name->string;

    JsonValue* extensions = jsonObjectFind(object, "extensions");
    CHECK(extensions && extensions->type == JSON_ARRAY);
    for (JsonArray* item = extensions->array; item; item = item->next) {
        CHECK(item->value->type == JSON_STRING);
        vector_push(syntax->file_exts, item->value->string);
    }
    vector_shrink(syntax->file_exts);

    JsonValue* comment = jsonObjectFind(object, "comment");
    if (comment && comment->type != JSON_NULL) {
        CHECK(comment->type == JSON_STRING);
        syntax->singleline_comment_start = comment->string;
    } else {
        syntax->singleline_comment_start = NULL;
    }

    JsonValue* multi_comment = jsonObjectFind(object, "multiline-comment");
    if (multi_comment && multi_comment->type != JSON_NULL) {
        CHECK(multi_comment->type == JSON_ARRAY);
        JsonValue* mcs = multi_comment->array->value;
        CHECK(mcs && mcs->type == JSON_STRING);
        syntax->multiline_comment_start = mcs->string;
        JsonValue* mce = multi_comment->array->next->value;
        CHECK(mce && mce->type == JSON_STRING);
        syntax->multiline_comment_end = mce->string;
        CHECK(multi_comment->array->next->next == NULL);
    } else {
        syntax->multiline_comment_start = NULL;
        syntax->multiline_comment_end = NULL;
    }
    const char* kw_fields[] = {"keywords1", "keywords2", "keywords3"};

    for (int i = 0; i < 3; i++) {
        JsonValue* keywords = jsonObjectFind(object, kw_fields[i]);
        if (keywords && keywords->type != JSON_NULL) {
            CHECK(keywords->type == JSON_ARRAY);
            for (JsonArray* item = keywords->array; item; item = item->next) {
                CHECK(item->value->type == JSON_STRING);
                vector_push(syntax->keywords[i], item->value->string);
            }
        }
        vector_shrink(syntax->keywords[i]);
    }

    // TODO: Add flags option in json file
    syntax->flags = HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS;

    syntax->arena = arena;

    // Add to HLDB
    if (!HLDB_tail) {
        gEditor.HLDB = HLDB_tail = syntax;
    } else {
        HLDB_tail->next = syntax;
        HLDB_tail = HLDB_tail->next;
    }

    return true;
END:
    arenaDeinit(&arena);
    free(syntax);
    return false;
}

void editorFreeHLDB(void) {
    EditorSyntax* HLDB = gEditor.HLDB;
    while (HLDB) {
        EditorSyntax* temp = HLDB;
        HLDB = HLDB->next;
        arenaDeinit(&temp->arena);
        free(temp->file_exts.data);
        free(temp->keywords[0].data);
        free(temp->keywords[1].data);
        free(temp->keywords[2].data);
        free(temp);
    }
    gEditor.HLDB = NULL;
    HLDB_tail = NULL;
}
