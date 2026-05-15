#include "utils.h"

#include <ctype.h>
#include <limits.h>

#include "os.h"
#include "terminal.h"

void panic(const char* file, int line, const char* s) {
    terminalExit();
#ifndef NDEBUG
    UNUSED(file);
    UNUSED(line);
    fprintf(stderr, "Fatal error: %s\r\n", s);
#else
    fprintf(stderr, "Fatal error at %s:%d: %s\r\n", file, line, s);
#endif
    exit(EXIT_FAILURE);
}

void* _malloc_s(const char* file, int line, size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size != 0)
        panic(file, line, "malloc");

    return ptr;
}

void* _calloc_s(const char* file, int line, size_t n, size_t size) {
    void* ptr = calloc(n, size);
    if (!ptr && size != 0)
        panic(file, line, "calloc");

    return ptr;
}

void* _realloc_s(const char* file, int line, void* ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (!ptr && size != 0)
        panic(file, line, "realloc");
    return ptr;
}

void _vector_make_room(_Vector* _vec, size_t item_size) {
    if (!_vec->capacity) {
        _vec->data = malloc_s(item_size * VECTOR_MIN_CAPACITY);
        _vec->capacity = VECTOR_MIN_CAPACITY;
    }
    if (_vec->size >= _vec->capacity) {
        _vec->capacity *= VECTOR_EXTEND_RATE;
        _vec->data = realloc_s(_vec->data, _vec->capacity * item_size);
    }
}

void abufAppendN(abuf* ab, const char* s, size_t n) {
    if (n == 0)
        return;

    if (ab->len + n > ab->capacity) {
        ab->capacity += n;
        ab->capacity *= ABUF_GROWTH_RATE;
        char* new = realloc_s(ab->buf, ab->capacity);
        ab->buf = new;
    }

    memcpy(&ab->buf[ab->len], s, n);
    ab->len += n;
}

void abufFree(abuf* ab) {
    free(ab->buf);
    ab->buf = NULL;
    ab->len = 0;
    ab->capacity = 0;
}

typedef struct {
    const char* name;
    int value;
} ColorStrIntPair;

static const ColorStrIntPair str_color_map[ANSI16_COUNT] = {
    [ANSI16_BLACK] = {"BLACK", 30},
    [ANSI16_RED] = {"RED", 31},
    [ANSI16_GREEN] = {"GREEN", 32},
    [ANSI16_YELLOW] = {"YELLOW", 33},
    [ANSI16_BLUE] = {"BLUE", 34},
    [ANSI16_MAGENTA] = {"MAGENTA", 35},
    [ANSI16_CYAN] = {"CYAN", 36},
    [ANSI16_WHITE] = {"WHITE", 37},
    [ANSI16_GRAY] = {"GRAY", 90},
    [ANSI16_BRIGHT_RED] = {"BRIGHT_RED", 91},
    [ANSI16_BRIGHT_GREEN] = {"BRIGHT_GREEN", 92},
    [ANSI16_BRIGHT_YELLOW] = {"BRIGHT_YELLOW", 93},
    [ANSI16_BRIGHT_BLUE] = {"BRIGHT_BLUE", 94},
    [ANSI16_BRIGHT_MAGENTA] = {"BRIGHT_MAGENTA", 95},
    [ANSI16_BRIGHT_CYAN] = {"BRIGHT_CYAN", 96},
    [ANSI16_BRIGHT_WHITE] = {"BRIGHT_WHITE", 97},
};

bool strToColor(const char* s, Color* out) {
    if (!s || !out || s[0] == '\0')
        return false;

    size_t len = strlen(s);
    switch (len) {
        // 256 color index
        case 1:
            if (!(s[0] >= '0' && s[0] <= '9'))
                return false;

            out->kind = COLOR_256;
            out->index = s[0] - '0';
            return true;

        case 2:
            if (!(s[0] >= '1' && s[0] <= '9'))
                return false;
            if (!(s[1] >= '0' && s[1] <= '9'))
                return false;

            out->kind = COLOR_256;
            out->index = (s[0] - '0') * 10;
            out->index += (s[1] - '0');
            return true;

        case 3:
            if (!(s[0] >= '1' && s[0] <= '9'))
                break;  // Might be "RED"
            if (!(s[1] >= '0' && s[1] <= '9'))
                return false;
            if (!(s[2] >= '0' && s[2] <= '9'))
                return false;

            {
                int index = (s[0] - '0') * 100;
                index += (s[1] - '0') * 10;
                index += (s[2] - '0');
                if (index > 255)
                    return false;

                out->kind = COLOR_256;
                out->index = index;
            }
            return true;

        case 6:
        case 7:
        case 8: {
            const char* hex_start = s;
            if (len == 8) {
                if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
                    break;
                hex_start += 2;
            } else if (len == 7) {
                if (s[0] != '#')
                    break;
                hex_start++;
            }

            // RGB hex string
            bool valid = true;
            for (int i = 0; i < 6; i++) {
                if (!((hex_start[i] >= '0' && hex_start[i] <= '9') ||
                      (hex_start[i] >= 'A' && hex_start[i] <= 'F') ||
                      (hex_start[i] >= 'a' && hex_start[i] <= 'f'))) {
                    valid = false;
                    break;
                }
            }

            if (!valid)
                break;

            out->kind = COLOR_RGB;

            unsigned int hex = strtoul(hex_start, NULL, 16);
            out->r = (hex >> 16) & 0xFF;
            out->g = (hex >> 8) & 0xFF;
            out->b = hex & 0xFF;
        }
            return true;

        default:
            break;
    }

    // Default
    if (strCaseCmp(s, "NONE") == 0 || strCaseCmp(s, "DEFAULT") == 0) {
        out->kind = COLOR_DEFAULT;
        return true;
    }

    // 16 color
    for (int i = 0; i < ANSI16_COUNT; i++) {
        if (strCaseCmp(s, str_color_map[i].name) == 0) {
            out->kind = COLOR_ANSI16;
            out->index = i;
            return true;
        }
    }

    return false;
}

int colorToStr(Color color, char buf[16]) {
    switch (color.kind) {
        case COLOR_DEFAULT:
            return snprintf(buf, 8, "DEFAULT");
        case COLOR_ANSI16:
            return snprintf(buf, 16, "%s", str_color_map[color.index].name);
        case COLOR_256:
            return snprintf(buf, 8, "%d", color.index);
        case COLOR_RGB:
            return snprintf(buf, 16, "%02x%02x%02x", color.r, color.g, color.b);
    }
    return 0;
}

static inline void appendColorParams(abuf* ab, Color color, bool is_bg) {
    switch (color.kind) {
        case COLOR_DEFAULT:
            abufAppendStr(ab, is_bg ? "49" : "39");
            break;
        case COLOR_ANSI16: {
            int code = str_color_map[color.index].value;
            if (is_bg)
                code += 10;
            char buf[16];
            int len = snprintf(buf, sizeof(buf), "%d", code);
            abufAppendN(ab, buf, len);
            break;
        }
        case COLOR_256: {
            char buf[16];
            int len = snprintf(buf, sizeof(buf), "%d;5;%d", is_bg ? 48 : 38,
                               color.index);
            abufAppendN(ab, buf, len);
            break;
        }
        case COLOR_RGB: {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "%d;2;%d;%d;%d",
                               is_bg ? 48 : 38, color.r, color.g, color.b);
            abufAppendN(ab, buf, len);
            break;
        }
    }
}

void setColor(abuf* ab, Color color, bool is_bg) {
    abufAppendStr(ab, "\x1b[");
    appendColorParams(ab, color, is_bg);
    abufAppendStr(ab, "m");
}

void setColors(abuf* ab, Color fg, Color bg) {
    abufAppendStr(ab, "\x1b[");
    appendColorParams(ab, fg, false);
    abufAppendStr(ab, ";");
    appendColorParams(ab, bg, true);
    abufAppendStr(ab, "m");
}

void gotoXY(abuf* ab, int x, int y) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", x, y);
    abufAppendN(ab, buf, len);
}

int isSeparator(int c) {
    return strchr("`~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?", c) != NULL;
}

int isNonSeparator(int c) {
    return !isSeparator(c);
}

int isSpace(int c) {
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

int isNonSpace(int c) {
    return !isSpace(c);
}

int isNonIdentifierChar(int c) {
    return isSpace(c) || c == '\0' || isSeparator(c);
}

int isIdentifierChar(int c) {
    return !isNonIdentifierChar(c);
}

char isOpenBracket(int key) {
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

char isCloseBracket(int key) {
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

int getDigit(int n) {
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

char* getBaseName(char* path) {
    char* file = path + strlen(path);
    for (; file > path; file--) {
        if (*file == '/'
#ifdef _WIN32
            || *file == '\\'
#endif
        ) {
            file++;
            break;
        }
    }
    return file;
}

char* getDirName(char* path) {
    char* name = getBaseName(path);
    if (name == path) {
        name = path;
        *name = '.';
        name++;
    } else {
        path--;
    }
    *name = '\0';
    return path;
}

// if path doesn't have a .EXT, append extension
void addDefaultExtension(char* path, const char* extension, int path_length) {
    char* src = path + strlen(path) - 1;

    while (!(*src == '/'
#ifdef _WIN32
             || *src == '\\'
#endif
             ) &&
           src > path) {
        if (*src == '.') {
            return;
        }
        src--;
    }

    strncat(path, extension, path_length);
}

int64_t getLine(char** lineptr, size_t* n, FILE* stream) {
    char* buf = NULL;
    size_t capacity;
    int64_t size = 0;
    int c;
    const size_t buf_size = 128;

    if (!lineptr || !stream || !n)
        return -1;

    buf = *lineptr;
    capacity = *n;

    c = fgetc(stream);
    if (c == EOF)
        return -1;

    if (!buf) {
        buf = malloc_s(buf_size);
        capacity = buf_size;
    }

    while (c != EOF) {
        if ((size_t)size > (capacity - 1)) {
            capacity += buf_size;
            buf = realloc_s(buf, capacity);
        }
        buf[size++] = c;

        if (c == '\n')
            break;

        c = fgetc(stream);
    }

    buf[size] = '\0';
    *lineptr = buf;
    *n = capacity;

    return size;
}

int strCaseCmp(const char* s1, const char* s2) {
    if (s1 == s2)
        return 0;

    int result;
    while ((result = tolower(*s1) - tolower(*s2)) == 0) {
        if (*s1 == '\0')
            break;
        s1++;
        s2++;
    }
    return result;
}

char* strCaseStr(const char* str, const char* sub_str) {
    // O(n*m), but should be ok
    if (*sub_str == '\0')
        return (char*)str;

    while (*str != '\0') {
        const char* s = str;
        const char* sub = sub_str;
        while (tolower(*s) == tolower(*sub)) {
            s++;
            sub++;
            if (*sub == '\0') {
                return (char*)str;
            }
        }
        str++;
    }

    return NULL;
}

int findSubstring(const char* haystack,
                  size_t haystack_len,
                  const char* needle,
                  size_t needle_len,
                  size_t start,
                  bool ignore_case) {
    if (needle_len == 0) {
        return (start <= haystack_len) ? (int)start : -1;
    }

    if (haystack_len < needle_len)
        return -1;

    size_t limit = haystack_len - needle_len;
    if (start > limit)
        return -1;

    for (size_t i = start; i <= limit; ++i) {
        size_t j = 0;
        for (; j < needle_len; ++j) {
            uint8_t hay = (uint8_t)haystack[i + j];
            uint8_t nee = (uint8_t)needle[j];
            if (ignore_case) {
                if (tolower(hay) != tolower(nee))
                    break;
            } else if (hay != nee) {
                break;
            }
        }
        if (j == needle_len)
            return (int)i;
    }

    return -1;
}

bool strToInt(const char* str, int* out) {
    if (!str || !out) {
        return false;
    }

    // Skip front spaces
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    int sign = 1;
    if (*str == '+' || *str == '-') {
        sign = (*str++ == '-') ? -1 : 1;
    }

    if (*str < '0' || *str > '9') {
        return false;
    }

    int result = 0;
    while (*str >= '0' && *str <= '9') {
        int digit = *str - '0';
        if (result > INT_MAX / 10 ||
            (result == INT_MAX / 10 && digit > INT_MAX % 10)) {
            // Overflow
            *out = (sign == -1) ? INT_MIN : INT_MAX;
            return true;
        }

        result = result * 10 + digit;
        str++;
    }

    // Skip trailing spaces
    while (*str != '\0') {
        if (*str != ' ' && *str != '\t') {
            return false;
        }
        str++;
    }

    *out = (sign == -1) ? -result : result;
    return true;
}

size_t base64Encode(const char* buf, size_t len, char* output) {
    const char* table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    size_t i;
    char* p = output;

    for (i = 0; i < (len / 3) * 3; i += 3) {
        uint8_t b0 = buf[i];
        uint8_t b1 = buf[i + 1];
        uint8_t b2 = buf[i + 2];

        *p++ = table[b0 >> 2];
        *p++ = table[(b0 & 0x3) << 4 | b1 >> 4];
        *p++ = table[(b1 & 0xF) << 2 | b2 >> 6];
        *p++ = table[b2 & 0x3F];
    }

    uint8_t remain = (len % 3);
    if (remain == 2) {
        uint8_t b0 = buf[i];
        uint8_t b1 = buf[i + 1];

        *p++ = table[b0 >> 2];
        *p++ = table[(b0 & 0x3) << 4 | b1 >> 4];
        *p++ = table[(b1 & 0xF) << 2];
        *p++ = '=';
    } else if (remain == 1) {
        uint8_t b0 = buf[i];

        *p++ = table[b0 >> 2];
        *p++ = table[(b0 & 0x3) << 4];
        *p++ = '=';
        *p++ = '=';
    }

    *p++ = '\0';

    return (size_t)(p - output);
}

bool writeConsoleAll(const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    while (len) {
        int n = writeConsole(p, len);
        if (n <= 0)
            return false;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}
