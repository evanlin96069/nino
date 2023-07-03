#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "utils.h"

typedef enum JsonType {
    JSON_ERROR,
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;
typedef struct JsonArray JsonArray;
typedef struct JsonObject JsonObject;

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
    struct JsonArray* next;
    JsonValue* value;
};

struct JsonObject {
    struct JsonObject* next;
    char* key;
    JsonValue* value;
};

JsonValue* jsonParse(const char* text, Arena* arena);
JsonValue* jsonObjectFind(const JsonObject* object, const char* key);

#endif
