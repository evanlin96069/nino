#include "highlight.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../resources/bundle.h"
#include "config.h"
#include "os.h"

#define JSON_IMPLEMENTATION
#define JSON_MALLOC malloc_s
#include "json.h"

void editorUpdateSyntax(EditorFile* file, EditorRow* row) {
    if (row->hl) {
        // realloc might returns NULL when row->size == 0
        memset(row->hl, HL_NORMAL, row->size);
    }

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
            if (i + scs_len <= row->size &&
                strncmp(&row->data[i], scs, scs_len) == 0) {
                memset(&row->hl[i], HL_COMMENT, row->size - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_COMMENT;
                if (i + mce_len <= row->size &&
                    strncmp(&row->data[i], mce, mce_len) == 0) {
                    memset(&row->hl[i], HL_COMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                }
                i++;
                continue;
            } else if (i + mcs_len <= row->size &&
                       strncmp(&row->data[i], mcs, mcs_len) == 0) {
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
            if ((isdigit((uint8_t)c) || c == '.') && prev_sep) {
                int start = i;
                i++;
                if (c == '0') {
                    if (i < row->size) {
                        if (row->data[i] == 'x' || row->data[i] == 'X') {
                            // hex
                            i++;
                            while (
                                i < row->size &&
                                (isdigit((uint8_t)row->data[i]) ||
                                 (row->data[i] >= 'a' && row->data[i] <= 'f') ||
                                 (row->data[i] >= 'A' &&
                                  row->data[i] <= 'F'))) {
                                i++;
                            }
                        } else if (row->data[i] >= '0' && row->data[i] <= '7') {
                            // oct
                            i++;
                            while (i < row->size && row->data[i] >= '0' &&
                                   row->data[i] <= '7') {
                                i++;
                            }
                        } else if (row->data[i] == '.') {
                            // float
                            i++;
                            while (i < row->size &&
                                   isdigit((uint8_t)row->data[i])) {
                                i++;
                            }
                        }
                    }
                } else {
                    while (i < row->size && isdigit((uint8_t)row->data[i])) {
                        i++;
                    }
                    if (c != '.' && i < row->size && row->data[i] == '.') {
                        i++;
                        while (i < row->size &&
                               isdigit((uint8_t)row->data[i])) {
                            i++;
                        }
                    }
                }
                if (c == '.' && i - start == 1)
                    continue;

                if (i < row->size &&
                    (row->data[i] == 'f' || row->data[i] == 'F'))
                    i++;
                if (i == row->size || isSeparator(row->data[i]) ||
                    isSpace(row->data[i]))
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
                    if (klen <= row->size - i &&
                        strncmp(&row->data[i], s->keywords[kw].data[j], klen) ==
                            0 &&
                        (i + klen == row->size ||
                         isNonIdentifierChar(row->data[i + klen]))) {
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
    for (i = row->size - 1; i >= 0; i--) {
        if (row->data[i] == ' ' || row->data[i] == '\t') {
            row->hl[i] = HL_BG_TRAILING << HL_FG_BITS;
        } else {
            break;
        }
    }
}

void editorSetSyntaxHighlight(EditorFile* file, EditorSyntax* syntax) {
    file->syntax = syntax;
    for (int i = 0; i < file->num_rows; i++) {
        editorUpdateSyntax(file, &file->row[i]);
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
            if ((is_ext && ext && strCaseCmp(ext, s->file_exts.data[i]) == 0) ||
                (!is_ext && strCaseStr(file->filename, s->file_exts.data[i]))) {
                editorSetSyntaxHighlight(file, s);
                return;
            }
        }
    }
}

#define ARENA_SIZE (1 << 12)

static JsonArena hldb_arena;

static void loadEditorConfigHLDB(void);
static void editorLoadBundledHLDB(void);

void editorInitHLDB(void) {
    json_arena_init(&hldb_arena, ARENA_SIZE);

    loadEditorConfigHLDB();
    editorLoadBundledHLDB();

    char path[EDITOR_PATH_MAX];
    snprintf(path, sizeof(path), PATH_CAT("%s", CONF_DIR, "syntax"),
             getenv(ENV_HOME));

    DirIter iter = dirFindFirst(path);
    if (iter.error)
        return;

    do {
        const char* filename = dirGetName(&iter);
        char file_path[EDITOR_PATH_MAX];
        int len = snprintf(file_path, sizeof(file_path), PATH_CAT("%s", "%s"),
                           path, filename);

        // This is just to suppress Wformat-truncation
        if (len < 0)
            continue;

        if (getFileType(file_path) == FT_REG) {
            const char* ext = strrchr(filename, '.');
            if (ext && strcmp(ext, ".json") == 0) {
                editorLoadHLDB(file_path);
            }
        }
    } while (dirNext(&iter));
    dirClose(&iter);
}

// Built-in syntax highlighting for config file
static void loadEditorConfigHLDB(void) {
    EditorSyntax* syntax = calloc_s(1, sizeof(EditorSyntax));

    syntax->file_type = EDITOR_NAME;
    vector_push(syntax->file_exts, EDITOR_RC_FILE);
    vector_push(syntax->file_exts, EDITOR_CONFIG_EXT);
    syntax->singleline_comment_start = "#";
    syntax->multiline_comment_start = NULL;
    syntax->multiline_comment_end = NULL;

    EditorConCmd* curr = gEditor.cvars;
    while (curr) {
        vector_push(syntax->keywords[curr->has_callback ? 0 : 1], curr->name);
        curr = curr->next;
    }

    for (int i = 0; i < EDITOR_COLOR_COUNT; i++) {
        vector_push(syntax->keywords[2], color_element_map[i].label);
    }

    syntax->flags = HL_HIGHLIGHT_STRINGS;

    // Add to HLDB
    syntax->next = gEditor.HLDB;
    gEditor.HLDB = syntax;
}

static bool editorLoadJsonHLDB(const char* json, EditorSyntax* syntax) {
    // Parse json
    JsonValue* value = json_parse(json, &hldb_arena);
    if (value->type != JSON_OBJECT) {
        return false;
    }

    // Get data
#define CHECK(boolean)    \
    do {                  \
        if (!(boolean))   \
            return false; \
    } while (0)

    JsonObject* object = value->object;

    JsonValue* name = json_object_find(object, "name");
    CHECK(name && name->type == JSON_STRING);
    syntax->file_type = name->string;

    JsonValue* extensions = json_object_find(object, "extensions");
    CHECK(extensions && extensions->type == JSON_ARRAY);
    for (size_t i = 0; i < extensions->array->size; i++) {
        JsonValue* item = extensions->array->data[i];
        CHECK(item->type == JSON_STRING);
        vector_push(syntax->file_exts, item->string);
    }
    vector_shrink(syntax->file_exts);

    JsonValue* comment = json_object_find(object, "comment");
    if (comment && comment->type != JSON_NULL) {
        CHECK(comment->type == JSON_STRING);
        syntax->singleline_comment_start = comment->string;
    } else {
        syntax->singleline_comment_start = NULL;
    }

    JsonValue* multi_comment = json_object_find(object, "multiline-comment");
    if (multi_comment && multi_comment->type != JSON_NULL) {
        CHECK(multi_comment->type == JSON_ARRAY);
        CHECK(multi_comment->array->size == 2);
        JsonValue* mcs = multi_comment->array->data[0];
        CHECK(mcs && mcs->type == JSON_STRING);
        syntax->multiline_comment_start = mcs->string;
        JsonValue* mce = multi_comment->array->data[1];
        CHECK(mce && mce->type == JSON_STRING);
        syntax->multiline_comment_end = mce->string;
    } else {
        syntax->multiline_comment_start = NULL;
        syntax->multiline_comment_end = NULL;
    }
    const char* kw_fields[] = {"keywords1", "keywords2", "keywords3"};

    for (int i = 0; i < 3; i++) {
        JsonValue* keywords = json_object_find(object, kw_fields[i]);
        if (keywords && keywords->type != JSON_NULL) {
            CHECK(keywords->type == JSON_ARRAY);
            for (size_t j = 0; j < keywords->array->size; j++) {
                JsonValue* item = keywords->array->data[j];
                CHECK(item->type == JSON_STRING);
                vector_push(syntax->keywords[i], item->string);
            }
        }
        vector_shrink(syntax->keywords[i]);
    }

    // TODO: Add flags option in json file
    syntax->flags = HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS;

    return true;
}

static void editorLoadBundledHLDB(void) {
    for (size_t i = 0; i < sizeof(bundle) / sizeof(bundle[0]); i++) {
        EditorSyntax* syntax = calloc_s(1, sizeof(EditorSyntax));
        if (editorLoadJsonHLDB(bundle[i], syntax)) {
            // Add to HLDB
            syntax->next = gEditor.HLDB;
            gEditor.HLDB = syntax;
        } else {
            free(syntax);
        }
    }
}

bool editorLoadHLDB(const char* path) {
    FILE* fp;
    size_t size;
    char* buffer;

    // Load file
    fp = openFile(path, "rb");
    if (!fp)
        return false;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buffer = calloc_s(1, size + 1);

    if (fread(buffer, size, 1, fp) != 1) {
        fclose(fp);
        free(buffer);
        return false;
    }
    fclose(fp);

    EditorSyntax* syntax = calloc_s(1, sizeof(EditorSyntax));
    if (editorLoadJsonHLDB(buffer, syntax)) {
        // Add to HLDB
        syntax->next = gEditor.HLDB;
        gEditor.HLDB = syntax;
    } else {
        free(syntax);
    }

    free(buffer);
    return true;
}

void editorFreeHLDB(void) {
    EditorSyntax* HLDB = gEditor.HLDB;
    while (HLDB) {
        EditorSyntax* temp = HLDB;
        HLDB = HLDB->next;
        free(temp->file_exts.data);
        for (size_t i = 0;
             i < sizeof(temp->keywords) / sizeof(temp->keywords[0]); i++) {
            free(temp->keywords[i].data);
        }
        free(temp);
    }
    json_arena_deinit(&hldb_arena);
    gEditor.HLDB = NULL;
}
