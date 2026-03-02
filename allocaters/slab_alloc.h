#pragma once

#include "includes.h"
#include "extern.h"
#include "paging.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "memory.h"

slab_t* DeleteSlab(slab_t* head, slab_t* target);
void InitSlabAlloc(uint64_t start);
void* kmalloc(uint64_t size);
void kfree(void* ptr, uint64_t size);
uint64_t CalculateBitMapSize(uint64_t i);