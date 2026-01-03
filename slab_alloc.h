#pragma once

#include <stdint.h>
#include "paging_bootstrap.h"
#include "buddy_alloc.h"

#define NUM_CACHE 2
typedef struct Slab
{
    void* start;
    uint32_t num_slots;
    uint32_t free_count;
    struct Slab* next;
    uint32_t bitmap_size;
    uint32_t bitmap[];
} Slab;

typedef struct 
{
    uint32_t size;
    Slab* full_slabs;
    Slab* partial_slabs;
    Slab* empty_slabs;
} Cache;

uint32_t AddKernelPages(uint32_t num_pages, pde_t* page_directory);
Cache* InitSlabCache(uint32_t start, pde_t* page_directory);
void* kmalloc(uint32_t size);
void kfree(void* ptr, uint32_t size);