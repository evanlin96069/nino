#ifndef JSON_H
#define JSON_H

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

#endif
