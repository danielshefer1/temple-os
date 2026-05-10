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

    // Byte-prefix until both pointers are 8-byte aligned (only possible
    // when their misalignment matches; otherwise fall back to bytewise).
    while (n && (((unsigned long)d | (unsigned long)s) & 7)) {
        if (((unsigned long)d & 7) != ((unsigned long)s & 7)) break;
        *d++ = *s++; n--;
    }

    if (n >= 8 && (((unsigned long)d | (unsigned long)s) & 7) == 0) {
        unsigned long* d64 = (unsigned long*)d;
        const unsigned long* s64 = (const unsigned long*)s;
        unsigned long qwords = n >> 3;
        for (unsigned long i = 0; i < qwords; i++) d64[i] = s64[i];
        d += qwords * 8;
        s += qwords * 8;
        n &= 7;
    }

    while (n--) *d++ = *s++;
    return dst;
}

static inline void* st_memset(void* dst, int c, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;

    while (n && ((unsigned long)d & 7)) { *d++ = (unsigned char)c; n--; }

    unsigned long v64 = (unsigned char)c;
    v64 |= v64 << 8; v64 |= v64 << 16; v64 |= v64 << 32;
    unsigned long qwords = n >> 3;
    unsigned long* d64 = (unsigned long*)d;
    for (unsigned long i = 0; i < qwords; i++) d64[i] = v64;
    d += qwords * 8;

    for (unsigned long i = 0; i < (n & 7); i++) d[i] = (unsigned char)c;
    return dst;
}
