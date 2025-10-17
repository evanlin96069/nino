#include "utils.h"

#include <ctype.h>
#include <limits.h>

#include "os.h"
#include "terminal.h"

void panic(const char *file, int line, const char *s) {
    terminalExit();
#ifdef _DEBUG
    UNUSED(file);
    UNUSED(line);
    fprintf(stderr, "Fatal error: %s\r\n", s);
#else
    fprintf(stderr, "Fatal error at %s:%d: %s\r\n", file, line, s);
#endif
    exit(EXIT_FAILURE);
}

void *_malloc_s(const char *file, int line, size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size != 0)
        panic(file, line, "malloc");

    return ptr;
}

void *_calloc_s(const char *file, int line, size_t n, size_t size) {
    void *ptr = calloc(n, size);
    if (!ptr && size != 0)
        panic(file, line, "calloc");

    return ptr;
}

void *_realloc_s(const char *file, int line, void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (!ptr && size != 0)
        panic(file, line, "realloc");
    return ptr;
}

void _vector_make_room(_Vector *_vec, size_t item_size) {
    if (!_vec->capacity) {
        _vec->data = malloc_s(item_size * VECTOR_MIN_CAPACITY);
        _vec->capacity = VECTOR_MIN_CAPACITY;
    }
    if (_vec->size >= _vec->capacity) {
        _vec->capacity *= VECTOR_EXTEND_RATE;
        _vec->data = realloc_s(_vec->data, _vec->capacity * item_size);
    }
}

void abufAppendN(abuf *ab, const char *s, size_t n) {
    if (n == 0)
        return;

    if (ab->len + n > ab->capacity) {
        ab->capacity += n;
        ab->capacity *= ABUF_GROWTH_RATE;
        char *new = realloc_s(ab->buf, ab->capacity);
        ab->buf = new;
    }

    memcpy(&ab->buf[ab->len], s, n);
    ab->len += n;
}

void abufFree(abuf *ab) {
    free(ab->buf);
    ab->buf = NULL;
    ab->len = 0;
    ab->capacity = 0;
}

static inline bool isValidColor(const char *color) {
    if (strlen(color) != 6)
        return false;
    for (int i = 0; i < 6; i++) {
        if (!((color[i] >= '0' && color[i] <= '9') ||
              (color[i] >= 'A' && color[i] <= 'F') ||
              (color[i] >= 'a' && color[i] <= 'f')))
            return false;
    }
    return true;
}

bool strToColor(const char *color, Color *out) {
    if (!isValidColor(color))
        return false;

    int shift = 16;
    unsigned int hex = strtoul(color, NULL, 16);
    out->r = (hex >> shift) & 0xFF;
    shift -= 8;
    out->g = (hex >> shift) & 0xFF;
    shift -= 8;
    out->b = (hex >> shift) & 0xFF;
    return true;
}

void setColor(abuf *ab, Color color, int is_bg) {
    char buf[32];
    int len;
    if (color.r == 0 && color.g == 0 && color.b == 0 && is_bg) {
        len = snprintf(buf, sizeof(buf), "%s", ANSI_DEFAULT_BG);
    } else {
        len = snprintf(buf, sizeof(buf), "\x1b[%d;2;%d;%d;%dm", is_bg ? 48 : 38,
                       color.r, color.g, color.b);
    }
    abufAppendN(ab, buf, len);
}

void gotoXY(abuf *ab, int x, int y) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1b[%d;%dH", x, y);
    abufAppendN(ab, buf, len);
}

int colorToStr(Color color, char buf[8]) {
    return snprintf(buf, 8, "%02x%02x%02x", color.r, color.g, color.b);
}

int isSeparator(int c) {
    return strchr("`~!@#$%^&*()-=+[{]}\\|;:'\",.<>/?", c) != NULL;
}

int isNonSeparator(int c) { return !isSeparator(c); }

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

int isNonSpace(int c) { return !isSpace(c); }

int isNonIdentifierChar(int c) {
    return isSpace(c) || c == '\0' || isSeparator(c);
}

int isIdentifierChar(int c) { return !isNonIdentifierChar(c); }

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

char *getBaseName(char *path) {
    char *file = path + strlen(path);
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

char *getDirName(char *path) {
    char *name = getBaseName(path);
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
void addDefaultExtension(char *path, const char *extension, int path_length) {
    char *src = path + strlen(path) - 1;

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

int64_t getLine(char **lineptr, size_t *n, FILE *stream) {
    char *buf = NULL;
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

int strCaseCmp(const char *s1, const char *s2) {
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

char *strCaseStr(const char *str, const char *sub_str) {
    // O(n*m), but should be ok
    if (*sub_str == '\0')
        return (char *)str;

    while (*str != '\0') {
        const char *s = str;
        const char *sub = sub_str;
        while (tolower(*s) == tolower(*sub)) {
            s++;
            sub++;
            if (*sub == '\0') {
                return (char *)str;
            }
        }
        str++;
    }

    return NULL;
}

int findSubstring(const char *haystack, size_t haystack_len, const char *needle,
                  size_t needle_len, size_t start, bool ignore_case) {
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

int strToInt(const char *str) {
    if (!str) {
        return 0;
    }

    // Skip front spaces
    while (*str == ' ' || *str == '\t') {
        str++;
    }

    int sign = 1;
    if (*str == '+' || *str == '-') {
        sign = (*str++ == '-') ? -1 : 1;
    }

    int result = 0;
    while (*str >= '0' && *str <= '9') {
        if (result > INT_MAX / 10 ||
            (result == INT_MAX / 10 && (*str - '0') > INT_MAX % 10)) {
            // Overflow
            return (sign == -1) ? INT_MIN : INT_MAX;
        }

        result = result * 10 + (*str - '0');
        str++;
    }

    result = sign * result;

    // Skip trailing spaces
    while (*str != '\0') {
        if (*str != ' ' && *str != '\t') {
            return 0;
        }
        str++;
    }

    return result;
}

// https://opensource.apple.com/source/QuickTimeStreamingServer/QuickTimeStreamingServer-452/CommonUtilitiesLib/base64.c

static const char basis_64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64Encode(const char *string, int len, char *output) {
    int i;
    char *p = output;

    for (i = 0; i < len - 2; i += 3) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        *p++ = basis_64[((string[i] & 0x3) << 4) |
                        ((int)(string[i + 1] & 0xF0) >> 4)];
        *p++ = basis_64[((string[i + 1] & 0xF) << 2) |
                        ((int)(string[i + 2] & 0xC0) >> 6)];
        *p++ = basis_64[string[i + 2] & 0x3F];
    }

    if (i < len) {
        *p++ = basis_64[(string[i] >> 2) & 0x3F];
        if (i == (len - 1)) {
            *p++ = basis_64[((string[i] & 0x3) << 4)];
            *p++ = '=';
        } else {
            *p++ = basis_64[((string[i] & 0x3) << 4) |
                            ((int)(string[i + 1] & 0xF0) >> 4)];
            *p++ = basis_64[((string[i + 1] & 0xF) << 2)];
        }
        *p++ = '=';
    }

    *p++ = '\0';
    return p - output;
}

bool writeConsoleAll(const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    while (len) {
        int n = writeConsole(p, len);
        if (n <= 0)
            return false;
        p += (size_t)n;
        len -= (size_t)n;
    }
    return true;
}
