#include "math.h"

uint64_t pow(uint64_t base, uint64_t exp) {
    uint64_t result = 1;
    for (uint64_t i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

uint64_t BiggestBit(uint64_t num) {
    uint64_t result = 0;
    for (uint64_t i = 63; i >= 0; i--) {
        if (num & (1ULL << i)) {
            result = i;
            break;
        }
    }
    return result;
}

bool IsPowerOfTwo(uint64_t num) {
    if (num == 0) return false;
    return (num & (num - 1)) == 0;
}