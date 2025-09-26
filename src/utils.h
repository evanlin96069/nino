#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Macros
#define _DO02(m, sep, x, y) m(x) sep m(y)
#define _DO03(m, sep, x, y, z) \
    m(x) sep m(y)              \
    sep m(z)

#define _DO_N(x01, x02, x03, N, ...) _DO##N
#define _MAP(m, sep, ...) _DO_N(__VA_ARGS__, 03, 02, 01)(m, sep, __VA_ARGS__)

#define _NOP(s) s

#define PATH_CAT(...) _MAP(_NOP, DIR_SEP, __VA_ARGS__)

#define UNUSED(x) (void)!(x)

// Panic
#define PANIC(s) panic(__FILE__, __LINE__, s)
void panic(const char* file, int line, const char* s);

// ANSI escape sequences
#define ANSI_CLEAR "\x1b[m"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_NOT_UNDERLINE "\x1b[24m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_NOT_INVERT "\x1b[27m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

// Allocate
#define malloc_s(size) _malloc_s(__FILE__, __LINE__, size)
#define calloc_s(n, size) _calloc_s(__FILE__, __LINE__, n, size)
#define realloc_s(ptr, size) _realloc_s(__FILE__, __LINE__, ptr, size)

void* _malloc_s(const char* file, int line, size_t size);
void* _calloc_s(const char* file, int line, size_t n, size_t size);
void* _realloc_s(const char* file, int line, void* ptr, size_t size);

// Vector
#define VECTOR_MIN_CAPACITY 16
#define VECTOR_EXTEND_RATE 1.5

#define VECTOR(type)     \
    struct {             \
        size_t size;     \
        size_t capacity; \
        type* data;      \
    }

typedef struct {
    size_t size;
    size_t capacity;
    void* data;
} _Vector;

void _vector_make_room(_Vector* _vec, size_t item_size);

#define vector_push(vec, val)                                       \
    do {                                                            \
        _vector_make_room((_Vector*)&(vec), sizeof((vec).data[0])); \
        (vec).data[(vec).size++] = (val);                           \
    } while (0)

#define vector_pop(vec) ((vec).data[--(vec).size])

#define vector_shrink(vec)                                             \
    do {                                                               \
        (vec).data =                                                   \
            realloc_s((vec).data, sizeof((vec).data[0]) * (vec).size); \
    } while (0)

// Abuf
#define ABUF_GROWTH_RATE 1.5f
#define ABUF_INIT \
    { NULL, 0, 0 }

typedef struct {
    char* buf;
    size_t len;
    size_t capacity;
} abuf;

void abufAppend(abuf* ab, const char* s);
void abufAppendN(abuf* ab, const char* s, size_t n);
void abufFree(abuf* ab);

// Color
typedef struct Color {
    int r, g, b;
} Color;

Color strToColor(const char* color);
int colorToStr(Color color, char buf[8]);
void setColor(abuf* ab, Color color, int is_bg);

// Separator
typedef int (*IsCharFunc)(int c);
int isSeparator(int c);
int isNonSeparator(int c);
int isNonIdentifierChar(int c);
int isIdentifierChar(int c);
int isSpace(int c);
int isNonSpace(int c);

// File
char* getBaseName(char* path);
char* getDirName(char* path);
void addDefaultExtension(char* path, const char* extension, int path_length);

// Misc
void gotoXY(abuf* ab, int x, int y);
int getDigit(int n);

// String
int64_t getLine(char** lineptr, size_t* n, FILE* stream);
int strCaseCmp(const char* s1, const char* s2);
char* strCaseStr(const char* str, const char* sub_str);
int strToInt(const char* str);

// Base64
static inline int base64EncodeLen(int len) { return ((len + 2) / 3 * 4) + 1; }

int base64Encode(const char* string, int len, char* output);

#endif
