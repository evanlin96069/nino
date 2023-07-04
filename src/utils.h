#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

#include "row.h"

#ifdef _WIN32
#define ENV_HOME "USERPROFILE"
#define CONF_DIR ".nino"
#define _SLASH "\\"
#else
#define ENV_HOME "HOME"
#define CONF_DIR ".config/nino"
#define _SLASH "/"
#endif

#define PATH_CAT(...) \
    _GET_MACRO(__VA_ARGS__, _PATH_CAT3, _PATH_CAT2)(__VA_ARGS__)
#define _GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define _PATH_CAT2(p1, p2) p1 _SLASH p2
#define _PATH_CAT3(p1, p2, p3) p1 _SLASH p2 _SLASH p3

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

// Arena
typedef struct Arena {
    size_t capacity;
    size_t size;
    uint8_t* data;
} Arena;

void arenaInit(Arena* arena, size_t capacity);
void* arenaAlloc(Arena* arena, size_t size);
#define arenaReset(arena)  \
    do {                   \
        (arena)->size = 0; \
    } while (0)
void arenaDeinit(Arena* arena);

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

#define vector_push(vec, val)                             \
    do {                                                  \
        _vector_make_room((_Vector*)&(vec), sizeof(val)); \
        (vec).data[(vec).size++] = (val);                 \
    } while (0)

#define vector_pop(vec) ((vec).data[--(vec).size])

#define vector_shrink(vec)                                              \
    do {                                                                \
        (vec).data = realloc_s(vec.data, sizeof(*vec.data) * vec.size); \
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

// IO
int osRead(char* buf, int n);

// UTF-8
int editorRowPreviousUTF8(EditorRow* row, int cx);
int editorRowNextUTF8(EditorRow* row, int cx);

// Row
int editorRowCxToRx(const EditorRow* row, int cx);
int editorRowRxToCx(const EditorRow* row, int rx);

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
int isNonSpace(int c);

// File
const char* getBaseName(const char* path);
char* getDirName(char* path);

// Misc
void gotoXY(abuf* ab, int x, int y);
int getDigit(int n);

#endif
