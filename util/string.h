#pragma once

#include "includes.h"
#include "memory.h"   // memset/memcpy live here; re-exported so callers that
                      // include "string.h" (the historical convention) keep working.

void itoa(uint64_t value, char* str, uint64_t base, uint64_t min_width);
void flip_str(char* str);
bool isdigit(char c);
bool isuppercasealpha(char c);
bool islowercasealpha(char c);
uint64_t char_to_digit(char c);
uint64_t atoi(char* str, uint64_t base);
void cpystr(char* source, char* dst);
int32_t strcmp(const char* str1, const char* str2);
int32_t strncmp(char* str1, char* str2, uint64_t n);
uint64_t strlen(const char* str);

// Bounded printf-into-buffer. Returns the number of bytes that would have
// been written had the buffer been unbounded (excluding the terminating NUL),
// always NUL-terminating when size > 0. Mirrors C99 snprintf return semantics.
// Supported conversions: %d, %u, %lu, %x, %lx, %s, %c, %%. No precision/flags.
int64_t ksnprintf(char* buf, uint64_t size, const char* fmt, ...);
int64_t kvsnprintf(char* buf, uint64_t size, const char* fmt, va_list args);