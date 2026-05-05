#pragma once
#include "includes.h"
#include "lock_types.h"

typedef struct slab_t
{
    void* start;
    uint64_t num_slots;
    uint64_t free_count;
    struct slab_t* next;
    uint64_t bitmap_size;
    uint64_t bitmap[];
} slab_t;

typedef struct cache_t
{
    uint64_t size;
    slab_t* full_slabs;
    slab_t* partial_slabs;
    slab_t* empty_slabs;
    spinlock_t lock;
} cache_t;
