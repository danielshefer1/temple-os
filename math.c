#include "math.h"

uint32_t pow(uint32_t base, uint32_t exp) {
    uint32_t result = 1;
    for (uint32_t i = 0; i < exp; i++) {
        result *= base;
    }
    return result;
}

uint32_t BiggestBit(uint32_t num) {
    uint32_t result = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (num & (1 << i)) {
            result = i;
        }
    }
    return result;
}

bool IsPowerOfTwo(uint32_t num) {
    if (num == 0) return false;
    return (num & (num - 1)) == 0;
}