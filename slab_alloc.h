#pragma once

#include "includes.h"
#include "paging.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "mem_ops.h"

slab_t* DeleteSlab(slab_t* head, slab_t* target);
void InitSlabAlloc(uint32_t start);
void* kmalloc(uint32_t size);
void kfree(void* ptr, uint32_t size);
uint32_t CalculateBitMapSize(uint32_t i);