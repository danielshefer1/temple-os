#include "memory.h"

void memset(void* address, uint8_t value, uint64_t size) {
    uint8_t* d = (uint8_t*)address;

    // Byte-prefix until 8-byte aligned.
    while (size && ((uintptr_t)d & 7)) { *d++ = value; size--; }

    // 64-bit body. Replicate the byte to a u64 once, then store in chunks.
    uint64_t v64 = (uint64_t)value;
    v64 |= v64 << 8;
    v64 |= v64 << 16;
    v64 |= v64 << 32;
    uint64_t qwords = size >> 3;
    uint64_t* d64 = (uint64_t*)d;
    for (uint64_t i = 0; i < qwords; i++) d64[i] = v64;

    // Byte tail (size & 7).
    d += qwords * 8;
    for (uint64_t i = 0; i < (size & 7); i++) d[i] = value;
}

int32_t memcmp(const void* ptr1, const void* ptr2, uint64_t num) {
    const uint8_t* p1 = (const uint8_t*)ptr1;
    const uint8_t* p2 = (const uint8_t*)ptr2;

    if (((uintptr_t)p1 | (uintptr_t)p2 | num) % 4 == 0) {
        const uint32_t* p1_32 = (const uint32_t*)p1;
        const uint32_t* p2_32 = (const uint32_t*)p2;
        uint32_t words = num / 4;

        for (uint32_t i = 0; i < words; i++) {
            if (p1_32[i] != p2_32[i]) {
                p1 = (const uint8_t*)&p1_32[i];
                p2 = (const uint8_t*)&p2_32[i];
                for (int j = 0; j < 4; j++) {
                    if (p1[j] != p2[j]) return (int32_t)p1[j] - (int32_t)p2[j];
                }
            }
        }
        return 0;
    }

    for (uint64_t i = 0; i < num; i++) {
        if (p1[i] != p2[i]) return (int32_t)p1[i] - (int32_t)p2[i];
    }

    return 0; 
}

void memcpy(void* dest, const void* src, uint64_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    // Byte-prefix until either pointer mismatch parity (=> can't use wider
    // loads/stores at all) or both pointers are 8-byte aligned.
    while (n && (((uintptr_t)d | (uintptr_t)s) & 7)) {
        // If d and s aren't congruent mod 8, no alignment we can reach;
        // fall through to the byte loop after this.
        if (((uintptr_t)d & 7) != ((uintptr_t)s & 7)) break;
        *d++ = *s++; n--;
    }

    if (n >= 8 && (((uintptr_t)d | (uintptr_t)s) & 7) == 0) {
        uint64_t* d64 = (uint64_t*)d;
        const uint64_t* s64 = (const uint64_t*)s;
        uint64_t qwords = n >> 3;
        for (uint64_t i = 0; i < qwords; i++) d64[i] = s64[i];
        d += qwords * 8;
        s += qwords * 8;
        n &= 7;
    }

    for (uint64_t i = 0; i < n; i++) d[i] = s[i];
}