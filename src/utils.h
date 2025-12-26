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
#define abufAppendStr(ab, s) abufAppendN((ab), (s), sizeof(s) - 1)
void abufFree(abuf* ab);
#define abufReset abufFree

// Color
typedef struct Color {
    int r, g, b;
} Color;

static inline bool colorEql(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

bool strToColor(const char* color, Color* out);
int colorToStr(Color color, char buf[8]);
void setColor(abuf* ab, Color color, bool is_bg);
void setColors(abuf* ab, Color fg, Color bg);

// Separator
typedef int (*IsCharFunc)(int c);
int isSeparator(int c);
int isNonSeparator(int c);
int isNonIdentifierChar(int c);
int isIdentifierChar(int c);
int isSpace(int c);
int isNonSpace(int c);
char isOpenBracket(int key);
char isCloseBracket(int key);

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
int findSubstring(const char* haystack,
                  size_t haystack_len,
                  const char* needle,
                  size_t needle_len,
                  size_t start,
                  bool ignore_case);
int strToInt(const char* str);

// Base64
static inline int base64EncodeLen(int len) {
    return ((len + 2) / 3 * 4) + 1;
}

int base64Encode(const char* string, int len, char* output);

// Write console
#define writeConsoleStr(s) writeConsole((s), sizeof(s) - 1)
bool writeConsoleAll(const void* buf, size_t len);

#endif
