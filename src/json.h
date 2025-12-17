#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Arena

typedef struct JsonArena JsonArena;
typedef struct JsonArenaBlock JsonArenaBlock;

struct JsonArenaBlock {
    uint32_t size;
    uint32_t capacity;
    void* data;
    JsonArenaBlock* next;
};

struct JsonArena {
    JsonArenaBlock* blocks;
    JsonArenaBlock* current_block;
    JsonArenaBlock* last_block;
    uint32_t first_block_size;
};

void json_arena_init(JsonArena* arena, size_t first_block_size);
void json_arena_deinit(JsonArena* arena);
void json_arena_reset(JsonArena* arena);

void* json_arena_alloc(JsonArena* arena, size_t size);
void* json_arena_realloc(JsonArena* arena,
                         void* ptr,
                         size_t old_size,
                         size_t size);

// Parser

typedef struct JsonValue JsonValue;
typedef struct JsonArray JsonArray;
typedef struct JsonObject JsonObject;

typedef enum JsonType {
    JSON_ERROR,
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

struct JsonValue {
    JsonType type;
    union {
        double number;
        bool boolean;
        char* string;
        JsonArray* array;
        JsonObject* object;
    };
};

struct JsonArray {
    size_t size;
    JsonValue** data;
};

struct JsonObject {
    struct JsonObject* next;
    char* key;
    JsonValue* value;
};

JsonValue* json_parse(const char* text, JsonArena* arena);

JsonValue* json_object_find(const JsonObject* object, const char* key);

#ifdef JSON_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef JSON_MALLOC
#define JSON_MALLOC malloc
#endif  // !JSON_MALLOC

#ifndef JSON_FREE
#define JSON_FREE free
#endif  // !JSON_FREE

// Arena

#define JSON_ARENA_ALIGNMENT 16

void json_arena_init(JsonArena* arena, size_t first_block_size) {
    memset(arena, 0, sizeof(JsonArena));
    arena->first_block_size = first_block_size;
}

void json_arena_deinit(JsonArena* arena) {
    JsonArenaBlock* curr = arena->blocks;
    while (curr) {
        JsonArenaBlock* temp = curr;
        curr = curr->next;
        JSON_FREE(temp->data);
        JSON_FREE(temp);
    }

    memset(arena, 0, sizeof(JsonArena));
}

void json_arena_reset(JsonArena* arena) {
    JsonArenaBlock* curr = arena->blocks;
    while (curr) {
        curr->size = 0;
        curr = curr->next;
    }
    arena->current_block = arena->blocks;
}

static inline size_t json__arena_alignment_loss(size_t bytes_allocated,
                                                size_t alignment) {
    size_t offset = bytes_allocated & (alignment - 1);
    if (offset == 0)
        return 0;
    return alignment - offset;
}

static inline size_t json__arena_block_bytes_left(JsonArenaBlock* blk) {
    size_t inc = json__arena_alignment_loss(blk->size, JSON_ARENA_ALIGNMENT);
    return blk->capacity - (blk->size + inc);
}

static void json__arena_alloc_new_block(JsonArena* arena,
                                        size_t requested_size) {
    size_t allocated_size;
    if (!arena->blocks) {
        allocated_size = arena->first_block_size;
    } else {
        allocated_size = arena->last_block->capacity;
    }

    if (allocated_size < 1)
        allocated_size = 1;

    while (allocated_size < requested_size) {
        allocated_size *= 2;
    }

    if (allocated_size > UINT32_MAX)
        allocated_size = UINT32_MAX;

    JsonArenaBlock* blk = JSON_MALLOC(sizeof(JsonArenaBlock));
    blk->data = JSON_MALLOC(allocated_size);
    blk->size = 0;
    blk->capacity = allocated_size;
    blk->next = NULL;

    if (!arena->blocks) {
        arena->blocks = blk;
    } else {
        arena->last_block->next = blk;
    }
    arena->last_block = blk;
    arena->current_block = blk;
}

void* json_arena_alloc(JsonArena* arena, size_t size) {
    if (size == 0)
        return NULL;

    if (!arena->blocks)
        json__arena_alloc_new_block(arena, size);

    while (json__arena_block_bytes_left(arena->current_block) < size) {
        arena->current_block = arena->current_block->next;
        if (!arena->current_block) {
            json__arena_alloc_new_block(arena, size);
            break;
        }
    }

    JsonArenaBlock* blk = arena->current_block;
    if (blk == NULL) {
        return NULL;
    }

    size_t inc = json__arena_alignment_loss(blk->size, JSON_ARENA_ALIGNMENT);
    void* out = (uint8_t*)blk->data + blk->size + inc;
    blk->size += size + inc;
    return out;
}

void* json_arena_realloc(JsonArena* arena,
                         void* ptr,
                         size_t old_size,
                         size_t size) {
    if (old_size >= size)
        return ptr;

    if (!ptr || !arena->blocks)
        return json_arena_alloc(arena, size);

    JsonArenaBlock* blk = arena->current_block;
    uint8_t* prev = (uint8_t*)blk->data + blk->size - old_size;
    uint32_t bytes_left = blk->capacity - blk->size + old_size;
    void* new_arr;
    if (prev == (uint8_t*)ptr && bytes_left >= size) {
        blk->size -= old_size;
        new_arr = json_arena_alloc(arena, size);
    } else {
        new_arr = json_arena_alloc(arena, size);
        memcpy(new_arr, ptr, old_size);
    }
    return new_arr;
}

// JSON

typedef struct JsonParserState {
    JsonArena* arena;
    const char* p;
    const char* start;
} JsonParserState;

#define JSON_ERROR_SIZE 64
#define JSON_STRING_SIZE 16
#define JSON_ARRAY_SIZE 16

typedef enum JsonTokenType {
    JSON_TOKEN_EMPTY = 0,
    JSON_TOKEN_ERROR,
    JSON_TOKEN_NULL,
    JSON_TOKEN_BOOLEAN,
    JSON_TOKEN_NUMBER,
    JSON_TOKEN_STRING,
    JSON_TOKEN_LBRACE = '{',
    JSON_TOKEN_RBRACE = '}',
    JSON_TOKEN_LBRACKET = '[',
    JSON_TOKEN_RBRACKET = ']',
    JSON_TOKEN_COMMA = ',',
    JSON_TOKEN_COLON = ':',
} JsonTokenType;

typedef struct JsonToken {
    JsonTokenType type;
    union {
        double number;
        bool boolean;
        char* string;
    };
} JsonToken;

static JsonToken json__token_error(JsonParserState* state,
                                   const char* fmt,
                                   ...) {
    JsonToken token;
    token.type = JSON_TOKEN_ERROR;
    token.string = json_arena_alloc(state->arena, JSON_ERROR_SIZE);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(token.string, JSON_ERROR_SIZE, fmt, ap);
    va_end(ap);

    return token;
}

static JsonToken json__next_token(JsonParserState* state, const char* text) {
    JsonToken token = {0};
    char c;

    if (text) {
        state->start = text;
        state->p = text;
    }

    if (!state->p) {
        return token;
    }

    while ((c = *state->p++)) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            continue;
        } else if ((c >= '0' && c <= '9') || c == '-') {
            token.type = JSON_TOKEN_NUMBER;
            double sign = 1;
            double value = 0.0f;
            double exp_sign = 10;
            double exp = 0.0f;
            if (c == '-') {
                sign = -1;
                c = *state->p++;
                if (!(c >= '0' && c <= '9')) {
                    return json__token_error(
                        state, "No number after minus sign at position %ld",
                        state->p - state->start);
                }
            }

            if (c == '0') {
                c = *state->p++;
            } else {
                while (c >= '0' && c <= '9') {
                    value *= 10;
                    value += c - '0';
                    c = *state->p++;
                }
            }

            if (c == '.') {
                c = *state->p++;
                if (c >= '0' && c <= '9') {
                    double i = 0.1;
                    while (c >= '0' && c <= '9') {
                        value += (c - '0') * i;
                        i *= 0.1;
                        c = *state->p++;
                    }
                } else {
                    return json__token_error(
                        state, "Unterminated fractional number at position %ld",
                        state->p - state->start);
                }
            }

            if (c == 'e' || c == 'E') {
                c = *state->p++;
                if (c == '-') {
                    exp_sign = 0.1;
                    c = *state->p++;
                } else if (c == '+') {
                    c = *state->p++;
                }

                if (c >= '0' && c <= '9') {
                    while (c >= '0' && c <= '9') {
                        exp *= 10;
                        exp += c - '0';
                        c = *state->p++;
                    }
                } else {
                    return json__token_error(
                        state,
                        "Exponent part is missing a number at position %ld",
                        state->p - state->start);
                }
            }
            state->p--;

            for (int i = 0; i < exp; i++) {
                value *= exp_sign;
            }
            token.number = sign * value;

            return token;
        } else if (c == '"') {
            token.type = JSON_TOKEN_STRING;
            token.string = json_arena_alloc(state->arena, JSON_STRING_SIZE);
            size_t capacity = JSON_STRING_SIZE;
            size_t size = 0;

            while ((c = *state->p++) != '"') {
                if (c == '\0') {
                    return json__token_error(
                        state, "Unterminated string at position %ld",
                        state->p - state->start);
                }

                // TODO: Add UTF-8 support
                if (c < 32 || c == 127) {
                    return json__token_error(
                        state,
                        "Bad control character in string literal "
                        "at position %ld",
                        state->p - state->start);
                }

                if (c == '\\') {
                    c = *state->p++;
                    switch (c) {
                        case '"':
                        case '\\':
                        case '/':
                            break;
                        case 'b':
                            c = '\b';
                            break;
                        case 'f':
                            c = '\f';
                            break;
                        case 'n':
                            c = '\n';
                            break;
                        case 'r':
                            c = '\r';
                            break;
                        case 't':
                            c = '\t';
                            break;
                        // TODO: Add Unicode
                        case 'u':
                            return json__token_error(
                                state, "Unicode escape not implemented");
                        default:
                            return json__token_error(
                                state, "Bad escaped character at position %ld",
                                state->p - state->start);
                    }
                }

                if (size + 2 > capacity) {
                    token.string = json_arena_realloc(
                        state->arena, token.string, capacity, capacity * 2);
                    capacity *= 2;
                }

                token.string[size] = c;
                size++;
            }

            token.string[size] = '\0';
            return token;
        } else if (strncmp("null", state->p - 1, strlen("null")) == 0) {
            state->p += strlen("null") - 1;
            token.type = JSON_TOKEN_NULL;
            return token;
        } else if (strncmp("true", state->p - 1, strlen("true")) == 0) {
            state->p += strlen("true") - 1;
            token.type = JSON_TOKEN_BOOLEAN;
            token.boolean = true;
            return token;
        } else if (strncmp("false", state->p - 1, strlen("false")) == 0) {
            state->p += strlen("false") - 1;
            token.type = JSON_TOKEN_BOOLEAN;
            token.boolean = false;
            return token;
        } else {
            switch (c) {
                case '{':
                    token.type = JSON_TOKEN_LBRACE;
                    break;
                case '}':
                    token.type = JSON_TOKEN_RBRACE;
                    break;
                case '[':
                    token.type = JSON_TOKEN_LBRACKET;
                    break;
                case ']':
                    token.type = JSON_TOKEN_RBRACKET;
                    break;
                case ',':
                    token.type = JSON_TOKEN_COMMA;
                    break;
                case ':':
                    token.type = JSON_TOKEN_COLON;
                    break;
                default:
                    return json__token_error(
                        state, "Unexpected token '%c' at position %ld", c,
                        state->p - state->start);
            }
            return token;
        }
    }

    state->start = NULL;
    state->p = NULL;

    return token;
}

static JsonValue* json__error(JsonParserState* state, const char* fmt, ...) {
    JsonValue* value = json_arena_alloc(state->arena, sizeof(JsonValue));
    value->type = JSON_ERROR;
    value->string = json_arena_alloc(state->arena, JSON_ERROR_SIZE);

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(value->string, JSON_ERROR_SIZE, fmt, ap);
    va_end(ap);

    return value;
}

static JsonValue* json__parse_array(JsonParserState* state);

static JsonValue* json__parse_object(JsonParserState* state);

static JsonValue* json__parse_value(JsonParserState* state, JsonToken token) {
    JsonValue* value;
    if (token.type == JSON_TOKEN_LBRACE) {
        value = json__parse_object(state);
    } else if (token.type == JSON_TOKEN_LBRACKET) {
        value = json__parse_array(state);
    } else {
        value = json_arena_alloc(state->arena, sizeof(JsonValue));
        switch (token.type) {
            case JSON_TOKEN_NULL:
                value->type = JSON_NULL;
                break;
            case JSON_TOKEN_BOOLEAN:
                value->type = JSON_BOOLEAN;
                value->boolean = token.boolean;
                break;
            case JSON_TOKEN_NUMBER:
                value->type = JSON_NUMBER;
                value->number = token.number;
                break;
            case JSON_TOKEN_STRING:
                value->type = JSON_STRING;
                value->string = token.string;
                break;
            case JSON_TOKEN_ERROR:
                value->type = JSON_ERROR;
                value->string = token.string;
                break;
            case JSON_TOKEN_EMPTY:
                return json__error(state, "Unexpected end");
                break;
            default:
                return json__error(state, "Unexpected token '%c'", token.type);
        }
    }
    return value;
}

static JsonValue* json__parse_object(JsonParserState* state) {
    JsonToken token = json__next_token(state, NULL);

    JsonObject head = {0};
    JsonObject* curr = &head;

    // TODO: Detect duplicated keys
    while (token.type == JSON_TOKEN_STRING) {
        curr->next = json_arena_alloc(state->arena, sizeof(JsonObject));
        curr = curr->next;
        curr->next = NULL;
        curr->key = token.string;

        token = json__next_token(state, NULL);
        if (token.type != JSON_TOKEN_COLON) {
            return json__error(state, "Expected ':' after property name");
        }

        token = json__next_token(state, NULL);
        curr->value = json__parse_value(state, token);
        if (curr->value->type == JSON_ERROR) {
            return curr->value;
        }

        token = json__next_token(state, NULL);
        if (token.type == JSON_TOKEN_RBRACE) {
            break;
        }

        if (token.type == JSON_TOKEN_COMMA) {
            token = json__next_token(state, NULL);
            if (token.type != JSON_TOKEN_STRING) {
                return json__error(state,
                                   "Expected double-quoted property name");
            }
        } else {
            return json__error(state,
                               "Expected ',' or '}' after property value");
        }
    }

    if (token.type != JSON_TOKEN_RBRACE) {
        return json__error(state, "Expected property name or '}'");
    }

    JsonValue* value = json_arena_alloc(state->arena, sizeof(JsonValue));
    value->type = JSON_OBJECT;
    value->object = head.next;
    return value;
}

static JsonValue* json__parse_array(JsonParserState* state) {
    JsonToken token = json__next_token(state, NULL);
    JsonArray* array = json_arena_alloc(state->arena, sizeof(JsonArray));
    size_t capacity = JSON_ARRAY_SIZE;
    array->data = json_arena_alloc(state->arena, sizeof(JsonValue*) * capacity);
    array->size = 0;

    while (token.type != JSON_TOKEN_RBRACKET) {
        if (array->size > 0) {
            if (token.type == JSON_TOKEN_COMMA) {
                token = json__next_token(state, NULL);
            } else {
                return json__error(state,
                                   "Expected ',' or ']' after array element");
            }
        }

        JsonValue* data = json__parse_value(state, token);
        if (data->type == JSON_ERROR)
            return data;

        if (array->size + 1 > capacity) {
            size_t new_capacity = capacity * 2;
            array->data = json_arena_realloc(state->arena, array->data,
                                             sizeof(JsonValue*) * capacity,
                                             sizeof(JsonValue*) * new_capacity);
            capacity = new_capacity;
        }

        array->data[array->size] = data;
        array->size++;

        token = json__next_token(state, NULL);
    }

    JsonValue* value = json_arena_alloc(state->arena, sizeof(JsonValue));
    value->type = JSON_ARRAY;
    value->array = array;
    return value;
}

JsonValue* json_parse(const char* text, JsonArena* arena) {
    JsonParserState state = {arena, NULL, NULL};
    JsonToken token = json__next_token(&state, text);
    JsonValue* value = json__parse_value(&state, token);
    if (value->type == JSON_ERROR)
        return value;

    // Check if any token left
    token = json__next_token(&state, NULL);
    if (token.type != JSON_TOKEN_EMPTY) {
        return json__error(&state, "Unexpected non-whitespace character");
    }
    return value;
}

JsonValue* json_object_find(const JsonObject* object, const char* key) {
    while (object) {
        if (strcmp(object->key, key) == 0) {
            return object->value;
        }
        object = object->next;
    }
    return NULL;
}

#endif  // JSON_IMPLEMENTATION

#endif  // !JSON_H
