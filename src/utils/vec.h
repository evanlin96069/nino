#ifndef VEC_H
#define VEC_H

#include "common.h"

#define VEC_MIN_CAPACITY 4
#define VEC_EXTEND_RATE 1.5f

#define Vec(type)        \
    struct {             \
        size_t size;     \
        type* data;      \
        size_t capacity; \
    }

typedef Vec(void) _Vec;

static inline void _vecReserve(_Vec* vec, size_t n, size_t item_size) {
    if (vec->size < n) {
        vec->capacity = n;
        vec->data = realloc_s(vec->data, vec->capacity * item_size);
    }
}

static inline void _vecMakeRoom(_Vec* vec, size_t n, size_t item_size) {
    if (n == 0 || item_size == 0)
        return;

    if (!vec->capacity) {
        vec->data = malloc_s(item_size * VEC_MIN_CAPACITY);
        vec->capacity = VEC_MIN_CAPACITY;
    }
    if (vec->size + n > vec->capacity) {
        vec->capacity += n;
        vec->capacity *= VEC_EXTEND_RATE;
        vec->data = realloc_s(vec->data, vec->capacity * item_size);
    }
}

#define _VEC_ITEM_SIZE(vec) \
    sizeof((vec)->data[0]) /* NOLINT(bugprone-sizeof-expression) */

#define vecReserve(vec, n) _vecReserve((_Vec*)(vec), (n), _VEC_ITEM_SIZE(vec));

#define vecPush(vec, ...)                                   \
    do {                                                    \
        _vecMakeRoom((_Vec*)(vec), 1, _VEC_ITEM_SIZE(vec)); \
        (vec)->data[(vec)->size++] = (__VA_ARGS__);         \
    } while (0)

#define vecPushAll(vec, arr, n)                                              \
    do {                                                                     \
        _vecMakeRoom((_Vec*)(vec), n, _VEC_ITEM_SIZE(vec));                  \
        memcpy(&(vec)->data[(vec)->size], (arr), (n) * _VEC_ITEM_SIZE(vec)); \
        (vec)->size += n;                                                    \
    } while (0)

#define vecPop(vec) ((vec)->data[--(vec)->size])

#define vecInsert(vec, index, ...)                              \
    do {                                                        \
        if ((index) > (vec)->size)                              \
            break;                                              \
        _vecMakeRoom((_Vec*)(vec), 1, _VEC_ITEM_SIZE(vec));     \
        memmove(&(vec)->data[(index) + 1], &(vec)->data[index], \
                _VEC_ITEM_SIZE(vec) * ((vec)->size - (index))); \
        (vec)->data[index] = (__VA_ARGS__);                     \
        (vec)->size++;                                          \
    } while (0)

#define vecErase(vec, index)                                            \
    do {                                                                \
        if ((index) < (vec)->size) {                                    \
            memmove(&(vec)->data[index], &(vec)->data[(index) + 1],     \
                    _VEC_ITEM_SIZE(vec) * ((vec)->size - (index) - 1)); \
            (vec)->size--;                                              \
        }                                                               \
    } while (0)

#define vecShrink(vec)                                                 \
    do {                                                               \
        (vec)->data =                                                  \
            realloc_s((vec)->data, _VEC_ITEM_SIZE(vec) * (vec)->size); \
        (vec)->capacity = (vec)->size;                                 \
    } while (0)

#define vecClear(vec)    \
    do {                 \
        (vec)->size = 0; \
    } while (0)

#define vecFree(vec)         \
    do {                     \
        free((vec)->data);   \
        (vec)->data = NULL;  \
        (vec)->size = 0;     \
        (vec)->capacity = 0; \
    } while (0)

#endif
