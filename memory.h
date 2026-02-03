#pragma once

#include "includes.h"

void memset(void* address, uint8_t value, uint64_t size);
int32_t memcmp(const void* ptr1, const void* ptr2, uint64_t num);
void memcpy(void* dest, const void* src, uint64_t n);