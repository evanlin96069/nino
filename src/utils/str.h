#ifndef STR_H
#define STR_H

#include "utils/vec.h"

typedef Vec(char) Str;
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

#define strReserve vecReserve
#define strPush vecPush
#define strPop vecPop
#define strInsert vecInsert
#define strErase vecErase
#define strShrink vecShrink
#define strClear vecClear
#define strFree vecFree

#define strAppendN vecPushAll

static inline void strAppend(Str* s, StrView sv) {
    vecPushAll(s, sv.data, sv.size);
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

static inline const char* strGetCStr(Str* s) {
    _vecMakeRoom((_Vec*)s, 1, sizeof(char));
    s->data[s->size] = '\0';
    return s->data;
}

#endif
