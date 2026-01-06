#pragma once

#include "includes.h"
#include "paging.h"
#include "types.h"
#include "defintions.h"

void memset(void* address, uint8_t value, uint32_t size);
void InitSlabAlloc(uint32_t start);
void* kmalloc(uint32_t size);
void kfree(void* ptr, uint32_t size);