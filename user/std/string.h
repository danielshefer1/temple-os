#pragma once

// Tiny string helpers — the freestanding userspace doesn't link a libc.
// Naming convention: st_* (stdtemple).

static inline unsigned long st_strlen(const char* s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

static inline int st_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static inline int st_strncmp(const char* a, const char* b, unsigned long n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static inline char* st_strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (char)c == 0 ? (char*)s : 0;
}

static inline void* st_memcpy(void* dst, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dst;
}

static inline void* st_memset(void* dst, int c, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;
    while (n--) *d++ = (unsigned char)c;
    return dst;
}
