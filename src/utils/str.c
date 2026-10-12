#include "str.h"

#include <limits.h>

#include "utils.h"

StrView svStripLeft(StrView str) {
    while (str.size > 0 && isSpace(str.data[0])) {
        str.data++;
        str.size--;
    }

    return str;
}

StrView svStripRight(StrView str) {
    while (str.size > 0 && isSpace(str.data[str.size - 1])) {
        str.size--;
    }

    return str;
}

StrView svStrip(StrView str) {
    return svStripRight(svStripLeft(str));
}

bool svEql(StrView s1, StrView s2) {
    if (s1.size != s2.size)
        return false;
    if (s1.data == s2.data)
        return true;
    for (size_t i = 0; i < s1.size; i++) {
        if (s1.data[i] != s2.data[i])
            return false;
    }
    return true;
}

bool svToInt(StrView str, int* out) {
    if (str.size == 0)
        return false;

    size_t i = 0;
    int sign = 1;
    if (str.data[i] == '+' || str.data[i] == '-') {
        sign = (str.data[i++] == '-') ? -1 : 1;
    }

    if (i == str.size || str.data[i] < '0' || str.data[i] > '9') {
        return false;
    }

    int result = 0;
    bool overflow = false;
    while (i < str.size && str.data[i] >= '0' && str.data[i] <= '9') {
        int digit = str.data[i] - '0';
        if (result > INT_MAX / 10 ||
            (result == INT_MAX / 10 && digit > INT_MAX % 10)) {
            overflow = true;
            break;
        }

        result = result * 10 + digit;
        i++;
    }

    if (i != str.size)
        return false;

    if (overflow)
        result = (sign == -1) ? INT_MIN : INT_MAX;
    else
        result *= sign;

    if (out)
        *out = result;
    return true;
}

void _strMakeRoom(Str* s, size_t n) {
    if (n == 0)
        return;

    if (!s->capacity) {
        s->data = malloc_s(STR_MIN_CAPACITY);
        s->capacity = STR_MIN_CAPACITY;
    }
    size_t need = s->size + n + 1;
    if (need > s->capacity) {
        s->capacity = need;
        s->capacity *= STR_EXTEND_RATE;
        s->data = realloc_s(s->data, s->capacity * sizeof(char));
    }
}
