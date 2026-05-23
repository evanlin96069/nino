#include "highlight.h"

#include "../resources/bundle.h"
#include "config.h"
#include "editor.h"
#include "os.h"

#define JSON_IMPLEMENTATION
#define JSON_MALLOC malloc_s
#include "json.h"

static uint32_t editorRowCountTrailingSpaces(const EditorRow* row) {
    uint32_t count = 0;
    for (int i = row->size - 1; i >= 0; i--) {
        if (!isSpace(row->data[i]))
            break;
        count++;
    }

    return count;
}

int editorUpdateSyntax(EditorFile* file, EditorRow* r, int flags) {
    const EditorSyntax* s = file->syntax;

    bool lazy = flags & HL_UPDATE_LAZY;
    bool single_line = flags & HL_UPDATE_SINGLE_LINE;

    if (!lazy)
        r->trailing_spaces = editorRowCountTrailingSpaces(r);

    if (!syntax.int_value || !s) {
        vector_clear(r->hl_spans);
        r->hl_updated = !lazy;
        return 1;
    }

    const char* scs = s->singleline_comment_start;
    const char* mcs = s->multiline_comment_start;
    const char* mce = s->multiline_comment_end;

    const int scs_len = scs ? strlen(scs) : 0;
    const int mcs_len = mcs ? strlen(mcs) : 0;
    const int mce_len = mce ? strlen(mce) : 0;

    bool do_next_row = true;
    int row_index = (int)(r - file->row);

    int processed_rows = 0;

    while (do_next_row && row_index < file->num_rows) {
        EditorRow* row = &file->row[row_index];

        vector_clear(row->hl_spans);
        row->hl_updated = !lazy;

        do_next_row = false;

        bool prev_sep = true;
        // TODO: support single-line comments/strings that end with '\' in C/C++
        bool in_comment =
            (row_index > 0 && file->row[row_index - 1].hl_open_comment);

        int i = 0;
        while (i < row->size) {
            char c = row->data[i];

            // Multi-line comment
            if (mcs_len && mce_len) {
                int start = i;
                if (!in_comment && i + mcs_len <= row->size &&
                    strncmp(&row->data[i], mcs, mcs_len) == 0) {
                    i += mcs_len;
                    in_comment = true;
                }

                if (in_comment) {
                    while (i < row->size) {
                        if (i + mce_len <= row->size &&
                            strncmp(&row->data[i], mce, mce_len) == 0) {
                            i += mce_len;
                            in_comment = false;
                            prev_sep = true;
                            break;
                        }
                        i++;
                    }

                    if (i > row->size)
                        i = row->size;

                    vector_push(row->hl_spans, (EditorHLSpan){
                                                   .start = start,
                                                   .len = i - start,
                                                   .type = HL_COMMENT,
                                               });
                    continue;
                }
            }

            if (lazy) {
                i++;
                continue;
            }

            // Single line comment
            if (scs_len) {
                if (i + scs_len <= row->size &&
                    strncmp(&row->data[i], scs, scs_len) == 0) {
                    // Mark entire line as comment
                    vector_push(row->hl_spans, (EditorHLSpan){
                                                   .start = i,
                                                   .len = row->size - i,
                                                   .type = HL_COMMENT,
                                               });
                    break;
                }
            }

            // String
            if (s->flags & HL_HIGHLIGHT_STRINGS) {
                if (c == '"' || c == '\'') {
                    int start = i;
                    i++;
                    while (i < row->size && row->data[i] != c) {
                        if (row->data[i] == '\\') {
                            i++;
                        }
                        i++;
                    }

                    if (i < row->size && row->data[i] == c)
                        i++;
                    if (i > row->size)
                        i = row->size;

                    vector_push(row->hl_spans, (EditorHLSpan){
                                                   .start = start,
                                                   .len = i - start,
                                                   .type = HL_STRING,
                                               });
                    prev_sep = true;
                    continue;
                }
            }

            // Number
            if (s->flags & HL_HIGHLIGHT_NUMBERS) {
                // Try to keep this simple and general, not tied too closely to
                // C/C++
                if ((isDigit(c) || c == '.') && prev_sep) {
                    int start = i;
                    enum NumberParseState {
                        NP_UNKNOWN,
                        NP_ACCEPT,
                        NP_REJECT,
                    } state = NP_UNKNOWN;
                    if (c == '0') {
                        i++;
                        if (i < row->size) {
                            if (row->data[i] == 'b' || row->data[i] == 'B') {
                                // Binary
                                i++;
                                while (i < row->size && (row->data[i] == '0' ||
                                                         row->data[i] == '1')) {
                                    i++;
                                }
                                state = (i - start > 2) ? NP_ACCEPT : NP_REJECT;
                            } else if (row->data[i] == 'x' ||
                                       row->data[i] == 'X') {
                                // Hex
                                i++;
                                while (i < row->size &&
                                       (isDigit(row->data[i]) ||
                                        (row->data[i] >= 'a' &&
                                         row->data[i] <= 'f') ||
                                        (row->data[i] >= 'A' &&
                                         row->data[i] <= 'F'))) {
                                    i++;
                                }
                                state = (i - start > 2) ? NP_ACCEPT : NP_REJECT;
                            } else {
                                // Oct
                                while (i < row->size && row->data[i] >= '0' &&
                                       row->data[i] <= '7') {
                                    i++;
                                }

                                if (i < row->size && row->data[i] != '.' &&
                                    row->data[i] != 'e' &&
                                    row->data[i] != 'E' &&
                                    !isDigit(row->data[i])) {
                                    // Make sure it's not a float
                                    state = NP_ACCEPT;
                                }
                            }
                        }
                    }

                    if (state == NP_UNKNOWN) {
                        bool is_float = false;
                        bool has_non_octal = false;

                        i = start;

                        // Float or decimal
                        while (i < row->size && isDigit(row->data[i])) {
                            if (c == '0' && i > start &&
                                (row->data[i] == '8' || row->data[i] == '9')) {
                                has_non_octal = true;
                            }
                            i++;
                        }

                        if (i < row->size && (row->data[i] == '.')) {
                            is_float = true;
                            i++;
                            while (i < row->size && isDigit(row->data[i])) {
                                i++;
                            }
                        }

                        if (c == '.' && i == start + 1) {
                            // Reject only '.'
                            state = NP_REJECT;
                        }

                        if (state == NP_UNKNOWN && i < row->size &&
                            (row->data[i] == 'e' || row->data[i] == 'E')) {
                            is_float = true;
                            i++;
                            if (i < row->size &&
                                (row->data[i] == '+' || row->data[i] == '-')) {
                                i++;
                            }
                            if (!(i < row->size && isDigit(row->data[i]))) {
                                state = NP_REJECT;
                            } else {
                                // Keep the state as NP_UNKNOWN to add suffixes
                                // later
                                while (i < row->size && isDigit(row->data[i])) {
                                    i++;
                                }
                            }
                        }

                        // Reject invalid octal integers that aren't floats
                        if (state == NP_UNKNOWN && c == '0' && has_non_octal &&
                            !is_float) {
                            state = NP_REJECT;
                        }

                        // We only allow float suffixes since they are common
                        if (state == NP_UNKNOWN && is_float && i < row->size &&
                            (row->data[i] == 'f' || row->data[i] == 'F')) {
                            i++;
                        }

                        if (state == NP_UNKNOWN) {
                            state = NP_ACCEPT;
                        }
                    }

                    if (i > row->size)
                        i = row->size;

                    if (state == NP_ACCEPT &&
                        (i == row->size || isSeparator(row->data[i]) ||
                         isSpace(row->data[i]))) {
                        vector_push(row->hl_spans, (EditorHLSpan){
                                                       .start = start,
                                                       .len = i - start,
                                                       .type = HL_NUMBER,
                                                   });
                    }
                    prev_sep = false;
                    continue;
                }
            }

            // Keyword
            if (prev_sep) {
                bool found_keyword = false;
                for (int kw = 0; kw < 3; kw++) {
                    for (size_t j = 0; j < s->keywords[kw].size; j++) {
                        int klen = strlen(s->keywords[kw].data[j]);
                        EditorHLType keyword_type = HL_KEYWORD1 + kw;
                        if (klen <= row->size - i &&
                            strncmp(&row->data[i], s->keywords[kw].data[j],
                                    klen) == 0 &&
                            (i + klen == row->size ||
                             isNonIdentifierChar(row->data[i + klen]))) {
                            found_keyword = true;
                            vector_push(row->hl_spans, (EditorHLSpan){
                                                           .start = i,
                                                           .len = klen,
                                                           .type = keyword_type,
                                                       });
                            i += klen;
                            break;
                        }
                    }
                    if (found_keyword) {
                        break;
                    }
                }

                if (found_keyword) {
                    prev_sep = false;
                    continue;
                }
            }
            prev_sep = !!isNonIdentifierChar(c);
            i++;
        }

        bool changed = (row->hl_open_comment != in_comment);
        row->hl_open_comment = in_comment;

        do_next_row = changed;
        row_index++;
        processed_rows++;

        if (single_line) {
            break;
        }
    }

    return processed_rows;
}

void editorFileReloadHighlight(EditorFile* file) {
    int i = 0;
    while (i < file->num_rows) {
        int count = editorUpdateSyntax(file, &file->row[i], HL_UPDATE_LAZY);
        if (count <= 0) {
            // This shouldn't happen
            break;
        }

        i += count;
    }
}

void editorSetSyntaxHighlight(EditorFile* file, EditorSyntax* syntax_def) {
    file->syntax = syntax_def;
    editorFileReloadHighlight(file);
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

    const char* home_dir = getEnv(ENV_HOME);
    if (!home_dir)
        return;

    char path[EDITOR_PATH_MAX];
    snprintf(path, sizeof(path), PATH_CAT("%s", CONF_DIR, "syntax"), home_dir);

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
    EditorSyntax* syntax_def = calloc_s(1, sizeof(EditorSyntax));

    syntax_def->file_type = EDITOR_NAME;
    vector_push(syntax_def->file_exts, EDITOR_RC_FILE);
    vector_push(syntax_def->file_exts, EDITOR_CONFIG_EXT);
    syntax_def->singleline_comment_start = "#";
    syntax_def->multiline_comment_start = NULL;
    syntax_def->multiline_comment_end = NULL;

    // Add commands
    ConCommandBase* curr = gEditor.cvars;
    while (curr) {
        vector_push(syntax_def->keywords[curr->is_command ? 0 : 1], curr->name);
        curr = curr->next;
    }

    // Add color labels
    for (int i = 0; i < UI_COLOR_COUNT; i++) {
        vector_push(syntax_def->keywords[2], color_element_map[i].label);
    }

    syntax_def->flags = HL_HIGHLIGHT_STRINGS;

    // Add to HLDB
    syntax_def->next = gEditor.HLDB;
    gEditor.HLDB = syntax_def;
}

static bool editorLoadJsonHLDB(const char* json, EditorSyntax* syntax_def) {
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
    // Name is required
    CHECK(name && name->type == JSON_STRING && *name->string != '\0');
    syntax_def->file_type = name->string;

    JsonValue* extensions = json_object_find(object, "extensions");
    // Extension is optional
    if (extensions) {
        CHECK(extensions->type == JSON_ARRAY);
        for (size_t i = 0; i < extensions->array->size; i++) {
            JsonValue* item = extensions->array->data[i];
            CHECK(item->type == JSON_STRING && *item->string != '\0');
            vector_push(syntax_def->file_exts, item->string);
        }
        vector_shrink(syntax_def->file_exts);
    }

    // Comment is optional
    JsonValue* comment = json_object_find(object, "comment");
    if (comment) {
        CHECK(comment->type == JSON_STRING && *comment->string != '\0');
        syntax_def->singleline_comment_start = comment->string;
    } else {
        syntax_def->singleline_comment_start = NULL;
    }

    // Multi-line comment is optional, but needs to come in pair
    JsonValue* multi_comment = json_object_find(object, "multiline-comment");
    if (multi_comment) {
        CHECK(multi_comment->type == JSON_ARRAY);
        CHECK(multi_comment->array->size == 2);
        JsonValue* mcs = multi_comment->array->data[0];
        CHECK(mcs && mcs->type == JSON_STRING && *mcs->string != '\0');
        syntax_def->multiline_comment_start = mcs->string;
        JsonValue* mce = multi_comment->array->data[1];
        CHECK(mce && mce->type == JSON_STRING && *mce->string != '\0');
        syntax_def->multiline_comment_end = mce->string;
    } else {
        syntax_def->multiline_comment_start = NULL;
        syntax_def->multiline_comment_end = NULL;
    }
    const char* kw_fields[] = {"keywords1", "keywords2", "keywords3"};

    for (int i = 0; i < 3; i++) {
        JsonValue* keywords = json_object_find(object, kw_fields[i]);
        if (keywords) {
            CHECK(keywords->type == JSON_ARRAY);
            for (size_t j = 0; j < keywords->array->size; j++) {
                JsonValue* item = keywords->array->data[j];
                CHECK(item->type == JSON_STRING && *item->string != '\0');
                vector_push(syntax_def->keywords[i], item->string);
            }
        }
        vector_shrink(syntax_def->keywords[i]);
    }

#undef CHECK

    // TODO: Add flags option in json file
    syntax_def->flags = HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS;

    return true;
}

static void editorLoadBundledHLDB(void) {
    for (size_t i = 0; i < sizeof(bundle) / sizeof(bundle[0]); i++) {
        EditorSyntax* syntax_def = calloc_s(1, sizeof(EditorSyntax));
        if (editorLoadJsonHLDB(bundle[i], syntax_def)) {
            // Add to HLDB
            syntax_def->next = gEditor.HLDB;
            gEditor.HLDB = syntax_def;
        } else {
            free(syntax_def);
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

    EditorSyntax* syntax_def = calloc_s(1, sizeof(EditorSyntax));
    if (editorLoadJsonHLDB(buffer, syntax_def)) {
        // Add to HLDB
        syntax_def->next = gEditor.HLDB;
        gEditor.HLDB = syntax_def;
    } else {
        free(syntax_def);
    }

    free(buffer);
    return true;
}

void editorFreeHLDB(void) {
    EditorSyntax* HLDB = gEditor.HLDB;
    while (HLDB) {
        EditorSyntax* temp = HLDB;
        HLDB = HLDB->next;
        vector_free(temp->file_exts);
        for (size_t i = 0;
             i < sizeof(temp->keywords) / sizeof(temp->keywords[0]); i++) {
            vector_free(temp->keywords[i]);
        }
        free(temp);
    }
    json_arena_deinit(&hldb_arena);
    gEditor.HLDB = NULL;
}
