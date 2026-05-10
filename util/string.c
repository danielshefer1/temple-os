#include "string.h"

void itoa(uint64_t value, char* str, uint64_t base, uint64_t min_width) {
    char* ptr = str;
    uint64_t tmp_value, count = 0;

    if (value == 0) {
        *ptr++ = '0';
        count++;
    } 
    else {
    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789ABCDEF"[tmp_value - value * base];
        count++;
    } while (value);
    }

    while (count < min_width) {
        *ptr++ = '0';
        count++;
    }

    *ptr = '\0';

    flip_str(str);
}

void flip_str(char* str) {
    char* start = str;
    char* end = str;

    while (*end != '\0') {
        end++;
    }
    end--;

    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
}

bool isdigit(char c) {
    if (c < '0' || c > '9') return false;
    return true;
}
bool isuppercasealpha(char c) {
    if (c < 'A' || c > 'F') return false;
    return true;
}
bool islowercasealpha(char c) {
    if (c < 'a' || c > 'f') return false;
    return true;
}

uint64_t char_to_digit(char c) {
    if (isdigit(c)) return c - '0';
    if (islowercasealpha(c)) return c - 'a' + 10;
    if (isuppercasealpha(c)) return c - 'A' + 10;
    return 0xFF;
}

uint64_t atoi(char* str, uint64_t base) {
    uint64_t result = 0, digit;
    char* ptr = str;

    while (*ptr != '\0') {
        result *= base;
        digit = char_to_digit(*ptr);
        if (digit == 0xFF) break;
        result += digit;
        ptr++;
    }
    return result;
}

void cpystr(char* source, char* dst) {
    uint64_t idx = 0;
    while (source[idx] != '\0') {
        dst[idx] = source[idx];
        idx++;
    }
    dst[idx] = '\0';
}

int32_t strcmp(const char* str1, const char* str2) {
    uint64_t idx = 0;
    while (str1[idx] != '\0' && str2[idx] != '\0') {
        if (str1[idx] > str2[idx]) return 1;
        if (str1[idx] < str2[idx]) return -1;
        idx++;
    }
    if (str1[idx] != '\0') return 1;
    if (str2[idx] != '\0') return -1;
    return 0;
}

int32_t strncmp(char* str1, char* str2, uint64_t n) {
    uint64_t idx = 0;
    while (str1[idx] != '\0' && str2[idx] != '\0' && idx < n) {
        if (str1[idx] > str2[idx]) return 1;
        if (str1[idx] < str2[idx]) return -1;
        idx++;
    }
    if (idx == n) return 0;
    if (str1[idx] != '\0') return 1;
    if (str2[idx] != '\0') return -1;
    return 0;
}

uint64_t strlen(const char* str) {
    if (str == NULL) return 0;
    uint64_t idx = 0;
    while (str[idx] != '\0') idx++;
    return idx;
}

// ---- ksnprintf: tiny printf-into-buffer for kernel use ----------------------
//
// Tracks the would-have-written count in `*total` so the return value matches
// C99 snprintf. `buf` may be NULL when size == 0, in which case we still
// compute the length so callers can two-pass-size their output.

static void ks_putc(char* buf, uint64_t size, uint64_t* total, char c) {
    if (*total + 1 < size) buf[*total] = c;
    (*total)++;
}

static void ks_puts(char* buf, uint64_t size, uint64_t* total, const char* s) {
    if (s == NULL) s = "(null)";
    while (*s) ks_putc(buf, size, total, *s++);
}

static void ks_putu(char* buf, uint64_t size, uint64_t* total,
                    uint64_t value, uint64_t base) {
    char tmp[32];
    char* p = tmp;
    if (value == 0) {
        *p++ = '0';
    } else {
        while (value) {
            uint64_t d = value % base;
            *p++ = (char)((d < 10) ? ('0' + d) : ('a' + d - 10));
            value /= base;
        }
    }
    while (p != tmp) {
        ks_putc(buf, size, total, *--p);
    }
}

static void ks_putd(char* buf, uint64_t size, uint64_t* total, int64_t value) {
    if (value < 0) {
        ks_putc(buf, size, total, '-');
        // Negate via unsigned to avoid UB on INT64_MIN.
        ks_putu(buf, size, total, (uint64_t)(-(value + 1)) + 1, 10);
    } else {
        ks_putu(buf, size, total, (uint64_t)value, 10);
    }
}

int64_t kvsnprintf(char* buf, uint64_t size, const char* fmt, va_list args) {
    if (fmt == NULL) return 0;
    uint64_t total = 0;

    while (*fmt) {
        if (*fmt != '%') { ks_putc(buf, size, &total, *fmt++); continue; }
        fmt++;

        // Optional 'l' length modifier — both %lu and %lx are 64-bit on x86_64,
        // and the unmodified %u/%x are too (callers in this kernel pass uint64_t
        // to %x already). Accept and ignore.
        bool is_long = false;
        if (*fmt == 'l') { is_long = true; fmt++; }
        (void)is_long;

        switch (*fmt) {
            case 'd':
            case 'i':
                ks_putd(buf, size, &total, (int64_t)va_arg(args, int64_t));
                break;
            case 'u':
                ks_putu(buf, size, &total, (uint64_t)va_arg(args, uint64_t), 10);
                break;
            case 'x':
                ks_putu(buf, size, &total, (uint64_t)va_arg(args, uint64_t), 16);
                break;
            case 's':
                ks_puts(buf, size, &total, va_arg(args, const char*));
                break;
            case 'c':
                ks_putc(buf, size, &total, (char)va_arg(args, int));
                break;
            case '%':
                ks_putc(buf, size, &total, '%');
                break;
            case '\0':
                goto done;
            default:
                // Unknown specifier: emit literally so the bug shows up in output.
                ks_putc(buf, size, &total, '%');
                ks_putc(buf, size, &total, *fmt);
                break;
        }
        fmt++;
    }
done:
    if (size > 0) {
        buf[(total < size) ? total : size - 1] = '\0';
    }
    return (int64_t)total;
}

int64_t ksnprintf(char* buf, uint64_t size, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int64_t r = kvsnprintf(buf, size, fmt, args);
    va_end(args);
    return r;
}

