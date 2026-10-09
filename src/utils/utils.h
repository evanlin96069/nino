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
