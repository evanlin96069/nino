#ifndef UTILS_H
#define UTILS_H

// Macros
#define _DO02(m, sep, x, y) m(x) sep m(y)
#define _DO03(m, sep, x, y, z) \
    m(x) sep m(y)              \
    sep m(z)

#define _DO_N(x01, x02, x03, N, ...) _DO##N
#define _MAP(m, sep, ...) _DO_N(__VA_ARGS__, 03, 02, 01)(m, sep, __VA_ARGS__)

#define _NOP(s) s

#define PATH_CAT(...) _MAP(_NOP, DIR_SEP, __VA_ARGS__)

// Vector
#define VECTOR_MIN_CAPACITY 4
#define VECTOR_EXTEND_RATE 1.5f

#define VECTOR(type)     \
    struct {             \
        size_t size;     \
        type* data;      \
        size_t capacity; \
    }

typedef VECTOR(void) _Vector;

static inline void _vector_reserve(_Vector* vec, size_t n, size_t item_size) {
    if (vec->size < n) {
        vec->capacity = n;
        vec->data = realloc_s(vec->data, vec->capacity * item_size);
    }
}

static inline void _vector_make_room(_Vector* vec, size_t n, size_t item_size) {
    if (item_size == 0)
        return;

    if (!vec->capacity) {
        vec->data = malloc_s(item_size * VECTOR_MIN_CAPACITY);
        vec->capacity = VECTOR_MIN_CAPACITY;
    }
    if (vec->size + n > vec->capacity) {
        vec->capacity += n;  // Ensure at least increase by n
        vec->capacity *= VECTOR_EXTEND_RATE;
        vec->data = realloc_s(vec->data, vec->capacity * item_size);
    }
}

// To suppress sizeof warning
#define _VECTOR_ITEM_SIZE(vec) \
    sizeof((vec)->data[0]) /* NOLINT(bugprone-sizeof-expression) */

#define vector_reserve(vec, n) \
    _vector_reserve((_Vector*)(vec), (n), _VECTOR_ITEM_SIZE(vec));

// Use __VA_ARGS__ so we can pass in compound literal
#define vector_push(vec, ...)                                          \
    do {                                                               \
        _vector_make_room((_Vector*)(vec), 1, _VECTOR_ITEM_SIZE(vec)); \
        (vec)->data[(vec)->size++] = (__VA_ARGS__);                    \
    } while (0)

#define vector_pushall(vec, arr, n)                                    \
    do {                                                               \
        _vector_make_room((_Vector*)(vec), n, _VECTOR_ITEM_SIZE(vec)); \
        memcpy(&(vec)->data[(vec)->size], (arr),                       \
               (n) * _VECTOR_ITEM_SIZE(vec));                          \
        (vec)->size += n;                                              \
    } while (0)

#define vector_pop(vec) ((vec)->data[--(vec)->size])

#define vector_insert(vec, index, ...)                                 \
    do {                                                               \
        if ((index) > (vec)->size)                                     \
            break;                                                     \
        _vector_make_room((_Vector*)(vec), 1, _VECTOR_ITEM_SIZE(vec)); \
        memmove(&(vec)->data[(index) + 1], &(vec)->data[index],        \
                _VECTOR_ITEM_SIZE(vec) * ((vec)->size - (index)));     \
        (vec)->data[index] = (__VA_ARGS__);                            \
        (vec)->size++;                                                 \
    } while (0)

#define vector_erase(vec, index)                                           \
    do {                                                                   \
        if ((index) < (vec)->size) {                                       \
            memmove(&(vec)->data[index], &(vec)->data[(index) + 1],        \
                    _VECTOR_ITEM_SIZE(vec) * ((vec)->size - (index) - 1)); \
            (vec)->size--;                                                 \
        }                                                                  \
    } while (0)

#define vector_shrink(vec)                                                \
    do {                                                                  \
        (vec)->data =                                                     \
            realloc_s((vec)->data, _VECTOR_ITEM_SIZE(vec) * (vec)->size); \
        (vec)->capacity = (vec)->size;                                    \
    } while (0)

#define vector_clear(vec) \
    do {                  \
        (vec)->size = 0;  \
    } while (0)

#define vector_free(vec)     \
    do {                     \
        free((vec)->data);   \
        (vec)->data = NULL;  \
        (vec)->size = 0;     \
        (vec)->capacity = 0; \
    } while (0)

// Str
typedef VECTOR(char) Str;

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

static inline StrView svFromStr(const Str s) {
    return (StrView){
        .size = s.size,
        .data = s.data,
    };
}

#define strReserve vector_reserve
#define strPush vector_push
#define strPop vector_pop
#define strInsert vector_insert
#define strErase vector_erase
#define strShrink vector_shrink
#define strClear vector_clear
#define strFree vector_free

#define strAppendN vector_pushall

static inline void strAppend(Str* s, StrView sv) {
    vector_pushall(s, sv.data, sv.size);
}

static inline Str strCopy(StrView sv) {
    Str s = {0};
    strAppend(&s, sv);
    return s;
}

// Color

typedef enum ColorANSI16 {
    ANSI16_BLACK = 0,
    ANSI16_RED,
    ANSI16_GREEN,
    ANSI16_YELLOW,
    ANSI16_BLUE,
    ANSI16_MAGENTA,
    ANSI16_CYAN,
    ANSI16_WHITE,
    ANSI16_GRAY,
    ANSI16_BRIGHT_RED,
    ANSI16_BRIGHT_GREEN,
    ANSI16_BRIGHT_YELLOW,
    ANSI16_BRIGHT_BLUE,
    ANSI16_BRIGHT_MAGENTA,
    ANSI16_BRIGHT_CYAN,
    ANSI16_BRIGHT_WHITE,

    ANSI16_COUNT,
} ColorANSI16;

typedef struct {
    const char* name;
    int value;
} ColorStrIntPair;

extern const ColorStrIntPair str_color_map[ANSI16_COUNT];

typedef union ColorRGBA {
    struct {
        uint8_t r, g, b, a;
    };
    uint32_t value;
} ColorRGBA;

typedef enum ColorKind {
    COLOR_DEFAULT,
    COLOR_ANSI16,
    COLOR_256,
    COLOR_RGB,
} ColorKind;

typedef struct Color {
    uint8_t kind;
    union {
        struct {
            uint8_t r, g, b;
        };
        uint8_t index;
    };
} Color;

static inline bool colorEql(Color a, Color b) {
    if (a.kind != b.kind)
        return false;
    switch (a.kind) {
        case COLOR_DEFAULT:
            return true;
        case COLOR_ANSI16:
        case COLOR_256:
            return a.index == b.index;
        case COLOR_RGB:
            return a.r == b.r && a.g == b.g && a.b == b.b;
        default:
            return false;
    }
}

bool strToColor(const char* s, Color* out);
int colorToStr(Color color, char buf[16]);
ColorRGBA colorToRGBA(Color color);

// File
char* getBaseName(char* path);
char* getDirName(char* path);
void addDefaultExtension(char* path, const char* extension, int path_length);

// String
int64_t getLine(char** lineptr, size_t* n, FILE* stream);
int strCaseCmp(const char* s1, const char* s2);
char* strCaseStr(const char* str, const char* sub_str);
int findSubstring(const char* haystack,
                  size_t haystack_len,
                  const char* needle,
                  size_t needle_len,
                  size_t start,
                  bool ignore_case);
bool strStartsWith(const char* s, const char* value, bool ignore_case);
bool strToInt(const char* str, int* out);

// Base64
static inline size_t base64EncodeLen(size_t len) {
    return ((len + 2) / 3 * 4) + 1;  // +1 for null terminator
}

// Returns length including null terminator
size_t base64Encode(const char* string, size_t len, char* output);

// ctype
typedef int (*IsCharFunc)(int c);

static inline int isSeparator(int c) {
    return strchr("`~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?", c) != NULL;
}

static inline int isNonSeparator(int c) {
    return !isSeparator(c);
}

static inline int isSpace(int c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\v':
        case '\f':
            return 1;
        default:
            return 0;
    }
}

static inline int isNonSpace(int c) {
    return !isSpace(c);
}

static inline int isNonIdentifierChar(int c) {
    return isSpace(c) || c == '\0' || isSeparator(c);
}

static inline int isIdentifierChar(int c) {
    return !isNonIdentifierChar(c);
}

static inline int isDigit(int c) {
    return c >= '0' && c <= '9';
}

static inline int isCntrl(int c) {
    // 0x7F is DEL
    return (c >= 0x00 && c <= 0x1F) || (c == 0x7F);
}

static inline int isLower(int c) {
    return (c >= 'a' && c <= 'z');
}

static inline int isUpper(int c) {
    return (c >= 'A' && c <= 'Z');
}

static inline int toLower(int c) {
    return isUpper(c) ? c + 32 : c;
}

static inline int toUpper(int c) {
    return isLower(c) ? c - 32 : c;
}

static inline char isOpenBracket(int key) {
    switch (key) {
        case '(':
            return ')';
        case '[':
            return ']';
        case '{':
            return '}';
        default:
            return 0;
    }
}

static inline char isCloseBracket(int key) {
    switch (key) {
        case ')':
            return '(';
        case ']':
            return '[';
        case '}':
            return '{';
        default:
            return 0;
    }
}

static inline int getDigit(int n) {
    if (n < 10)
        return 1;
    if (n < 100)
        return 2;
    if (n < 1000)
        return 3;
    if (n < 10000000) {
        if (n < 1000000) {
            if (n < 10000)
                return 4;
            return 5 + (n >= 100000);
        }
        return 7;
    }
    if (n < 1000000000)
        return 8 + (n >= 100000000);
    return 10;
}

#endif
