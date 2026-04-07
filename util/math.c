#include "math.h"

uint64_t pow(uint64_t base, uint64_t exp) {
    uint64_t result = 1;
    for (uint64_t i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

uint64_t BiggestBit(uint64_t num) {
    return 63 - __builtin_clzll(num);
}

uint64_t SmallestBit(uint64_t num) {
    return __builtin_ctzll(num);
}

bool IsPowerOfTwo(uint64_t num) {
    if (num == 0) return false;
    return (num & (num - 1)) == 0;
}

int64_t FindFirstUnsetInBuffer(uint64_t* buffer, uint64_t size) {
    for (uint64_t i = 0; i < size; i++) {
        if (buffer[i] != UINT64_MAX) {
            uint8_t bit = SmallestBit(~buffer[i]);
            return i * 64 + bit;
        }
    }
    return -1;
}