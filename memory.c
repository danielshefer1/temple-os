#include "memory.h"

void memset(void* address, uint8_t value, uint64_t size) {
    uint64_t reminder = size % 4;
    size -= reminder;
    size /= 4;
    uint32_t value_32 = value + (value << 8) + (value << 16) + (value << 24);

    for(uint64_t i = 0; i < size; i++) {
        ((uint32_t*) address)[i] = value_32;
    }

    for(uint64_t i = 0; i < reminder; i++) {
        ((uint8_t*) address)[size * 4 +i] = value;
    }
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
    uint8_t* dest1 = (uint8_t*)dest;
    const uint8_t* src1 = (const uint8_t*)src;

    if (((uintptr_t)dest1 | (uintptr_t)src1 | n) % 4 == 0) {
        uint64_t* p1_32 = (uint64_t*)dest1;
        const uint64_t* p2_32 = (const uint64_t*)src1;
        uint64_t words = n / 4;

        for (uint64_t i = 0; i < words; i++) {
            p1_32[i] = p2_32[i];
        }
        return;
    }

    for (uint64_t i = 0; i < n; i++) {
        dest1[i] = src1[i];
    }

    return; 
}