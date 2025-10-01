#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) (void)!(x)

// Panic
#define PANIC(s) panic(__FILE__, __LINE__, s)
void panic(const char* file, int line, const char* s);

// Allocate
#define malloc_s(size) _malloc_s(__FILE__, __LINE__, size)
#define calloc_s(n, size) _calloc_s(__FILE__, __LINE__, n, size)
#define realloc_s(ptr, size) _realloc_s(__FILE__, __LINE__, ptr, size)

void* _malloc_s(const char* file, int line, size_t size);
void* _calloc_s(const char* file, int line, size_t n, size_t size);
void* _realloc_s(const char* file, int line, void* ptr, size_t size);

#endif
