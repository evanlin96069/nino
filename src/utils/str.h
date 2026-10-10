#ifndef STR_H
#define STR_H

#define STR_MIN_CAPACITY 16
#define STR_EXTEND_RATE 1.5f

#include "utils/vec.h"

typedef struct Str {
    size_t size;
    char* data;
    size_t capacity;
} Str;

typedef Vec(Str) VecStr;

typedef struct StrView {
    size_t size;
    const char* data;
} StrView;

static inline StrView svFromCStr(const char* c_str) {
    return (StrView){
        .size = strlen(c_str),
        .data = c_str,
    };
}

static inline StrView svFromStr(Str s) {
    return (StrView){
        .size = s.size,
        .data = s.data,
    };
}

static inline bool svEql(StrView s1, StrView s2) {
    if (s1.size != s2.size)
        return false;
    if (s1.data == s2.data)
        return true;
    for (size_t i = 0; i < s1.size; i++) {
        if (s1.data[i] != s2.data[i])
            return false;
    }
    return true;
}

static inline void strReserve(Str* s, size_t n) {
    if (s->capacity < n + 1) {
        s->capacity = n + 1;
        s->data = realloc_s(s->data, s->capacity);
        s->data[s->size] = '\0';
    }
}

static inline void _strMakeRoom(Str* s, size_t n) {
    if (n == 0)
        return;

    if (!s->capacity) {
        s->data = malloc_s(STR_MIN_CAPACITY);
        s->capacity = STR_MIN_CAPACITY;
    }
    size_t need = s->size + n + 1;
    if (need > s->capacity) {
        s->capacity = need;
        s->capacity *= STR_EXTEND_RATE;
        s->data = realloc_s(s->data, s->capacity * sizeof(char));
    }
}

static inline void strAppendN(Str* s, const char* data, size_t n) {
    if (n == 0)
        return;
    _strMakeRoom(s, n);
    memcpy(&s->data[s->size], data, n);
    s->size += n;
    s->data[s->size] = '\0';
}

static inline void strAppend(Str* s, StrView sv) {
    strAppendN(s, sv.data, sv.size);
}

static inline void strPush(Str* s, char c) {
    _strMakeRoom(s, 1);
    s->data[s->size++] = c;
    s->data[s->size] = '\0';
}

static inline char strPop(Str* s) {
    char c = s->data[--s->size];
    s->data[s->size] = '\0';
    return c;
}

static inline void strInsert(Str* s, size_t index, char c) {
    if (index > s->size)
        return;
    _strMakeRoom(s, 1);
    memmove(&s->data[index + 1], &s->data[index], s->size - index + 1);
    s->data[index] = c;
    s->size++;
}

static inline void strErase(Str* s, size_t index) {
    if (index >= s->size)
        return;
    memmove(&s->data[index], &s->data[index + 1], s->size - index);
    s->size--;
}

static inline void strClear(Str* s) {
    s->size = 0;
    if (s->data)
        s->data[0] = '\0';
}

static inline void strShrink(Str* s) {
    if (!s->data)
        return;
    s->capacity = s->size + 1;
    s->data = realloc_s(s->data, s->capacity);
}

static inline void strFree(Str* s) {
    free(s->data);
    s->data = NULL;
    s->size = 0;
    s->capacity = 0;
}

static inline Str strCopy(StrView sv) {
    Str s = {0};
    strAppend(&s, sv);
    return s;
}

static inline Str strFromOwnedCStr(char* c_str) {
    size_t len = strlen(c_str);
    return (Str){
        .size = len,
        .data = c_str,
        .capacity = len + 1,
    };
}

static inline const char* strGetCStr(const Str* s) {
    return s->data ? s->data : "";
}

#endif
