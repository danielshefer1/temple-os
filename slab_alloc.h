#pragma once

#include "imports.h"

void* kmalloc(uint32_t size);
void kfree(void* ptr);

typedef struct Slab
{
    void* start;
    uint32_t num_slots;
    uint32_t free_count;
    uint32_t bitmap[];
} Slab;

typedef struct 
{
    uint32_t size;
    Slab* slabs;
} Cache;
