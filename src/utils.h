#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

#include "row.h"

// ANSI escape sequences
#define ANSI_CLEAR "\x1b[m"
#define ANSI_UNDERLINE "\x1b[4m"
#define ANSI_NOT_UNDERLINE "\x1b[24m"
#define ANSI_INVERT "\x1b[7m"
#define ANSI_NOT_INVERT "\x1b[27m"
#define ANSI_DEFAULT_FG "\x1b[39m"
#define ANSI_DEFAULT_BG "\x1b[49m"

// Allocate
void* malloc_s(size_t size);
void* calloc_s(size_t n, size_t size);
void* realloc_s(void* ptr, size_t size);

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
