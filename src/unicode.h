#ifndef UNICODE_H
#define UNICODE_H

#include <stddef.h>
#include <stdint.h>

uint32_t decodeUTF8(const char* str, size_t len, size_t* byte_size);
int unicodeWidth(uint32_t ucs);

#endif
