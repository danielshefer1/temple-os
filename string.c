#include "string.h"

void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width) {
    char* ptr = str;
    uint32_t tmp_value, count = 0;

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

uint32_t char_to_digit(char c) {
    if (isdigit(c)) return c - '0';
    if (islowercasealpha(c)) return c - 'a' + 10;
    if (isuppercasealpha(c)) return c - 'A' + 10;
    return 0xFF;
}

uint32_t atoi(char* str, uint32_t base) {
    uint32_t result = 0, digit;
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
    uint32_t idx = 0;
    while (source[idx] != '\0') {
        dst[idx] = source[idx];
        idx++;
    }
    dst[idx] = '\0';
}

int32_t strcmp(char* str1, char* str2) {
    uint32_t idx = 0;
    while (str1[idx] != '\0' && str2[idx] != '\0') {
        if (str1[idx] > str2[idx]) return 1;
        if (str1[idx] < str2[idx]) return -1;
        idx++;
    }
    if (str1[idx] != '\0') return 1;
    if (str2[idx] != '\0') return -1;
    return 0;
}

int32_t strncmp(char* str1, char* str2, uint32_t n) {
    uint32_t idx = 0;
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

uint32_t strlen(char* str) {
    uint32_t idx = 0;
    while (str[idx] != '\0') idx++;
    return idx;
}

