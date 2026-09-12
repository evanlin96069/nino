#ifndef UNICODE_H
#define UNICODE_H

int encodeUTF8(unsigned int code_point, char output[4]);
uint32_t decodeUTF8(const char* str, size_t len, size_t* byte_size);
int unicodeWidth(uint32_t ucs);
int strUTF8Width(const char* str);

#endif
