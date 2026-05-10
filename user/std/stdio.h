#pragma once

#include "std/syscalls.h"
#include "std/string.h"

// Tiny print helpers. All writes go through sys_write, no buffering.

static inline void st_puts_fd(long fd, const char* s) {
    sys_write(fd, s, st_strlen(s));
}

static inline void st_puts(const char* s) { st_puts_fd(1, s); }

static inline void st_putn_fd(long fd, unsigned long v) {
    char tmp[24];
    int t = 0;
    if (v == 0) tmp[t++] = '0';
    else { while (v) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; } }
    char out[24];
    int n = 0;
    while (t) out[n++] = tmp[--t];
    sys_write(fd, out, n);
}

static inline void st_putn(unsigned long v) { st_putn_fd(1, v); }

// Signed decimal.
static inline void st_putd_fd(long fd, long v) {
    if (v < 0) {
        sys_write(fd, "-", 1);
        st_putn_fd(fd, (unsigned long)(-v));
    } else {
        st_putn_fd(fd, (unsigned long)v);
    }
}

static inline void st_putd(long v) { st_putd_fd(1, v); }

// Octal — used by stat to print mode bits.
static inline void st_puto_fd(long fd, unsigned long v) {
    char tmp[24];
    int t = 0;
    if (v == 0) tmp[t++] = '0';
    else { while (v) { tmp[t++] = (char)('0' + (v & 7)); v >>= 3; } }
    char out[24];
    int n = 0;
    while (t) out[n++] = tmp[--t];
    sys_write(fd, out, n);
}

static inline void st_puto(unsigned long v) { st_puto_fd(1, v); }
