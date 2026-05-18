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
#define VECTOR_EXTEND_RATE 1.5

#define VECTOR(type)       \
    struct {               \
        uint32_t size;     \
        uint32_t capacity; \
        type* data;        \
    }

typedef VECTOR(void) _Vector;

void _vector_make_room(_Vector* _vec, size_t item_size);

// Use __VA_ARGS__ so we can pass in compound literal
#define vector_push(vec, ...)                                       \
    do {                                                            \
        _vector_make_room((_Vector*)&(vec), sizeof((vec).data[0])); \
        (vec).data[(vec).size++] = (__VA_ARGS__);                   \
    } while (0)

#define vector_pop(vec) ((vec).data[--(vec).size])

#define vector_shrink(vec)                                             \
    do {                                                               \
        (vec).data =                                                   \
            realloc_s((vec).data, sizeof((vec).data[0]) * (vec).size); \
    } while (0)

#define vector_clear(vec) \
    do {                  \
        (vec).size = 0;   \
    } while (0)

#define vector_free(vec)    \
    do {                    \
        free((vec).data);   \
        (vec).data = NULL;  \
        (vec).size = 0;     \
        (vec).capacity = 0; \
    } while (0)

// Str
typedef struct Str {
    char* data;
    int size;
} Str;

// Abuf
#define ABUF_GROWTH_RATE 1.5f
#define ABUF_INIT \
    { NULL, 0, 0 }

typedef struct {
    char* buf;
    size_t len;
    size_t capacity;
} abuf;

void abufAppendN(abuf* ab, const char* s, size_t n);
#define abufAppendStr(ab, s) abufAppendN((ab), (s), strlen(s))
void abufFree(abuf* ab);
#define abufReset abufFree

// Color

typedef enum ANSI16Color {
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
} ANSI16Color;

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
void setColor(abuf* ab, Color color, bool is_bg);
void setColors(abuf* ab, Color fg, Color bg);

// File
char* getBaseName(char* path);
char* getDirName(char* path);
void addDefaultExtension(char* path, const char* extension, int path_length);

// Misc
void gotoXY(abuf* ab, int x, int y);

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
bool strToInt(const char* str, int* out);

// Base64
static inline size_t base64EncodeLen(size_t len) {
    return ((len + 2) / 3 * 4) + 1;  // +1 for null terminator
}

// Returns length including null terminator
size_t base64Encode(const char* string, size_t len, char* output);

// Write console
#define writeConsoleStr(s) writeConsole((s), sizeof(s) - 1)
bool writeConsoleAll(const void* buf, size_t len);

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
