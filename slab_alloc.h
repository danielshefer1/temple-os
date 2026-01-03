#pragma once

#include <stdint.h>

void* kmalloc(uint32_t size);
void kfree(void* ptr);