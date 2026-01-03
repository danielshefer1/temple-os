#include "slab_alloc.h"

void InitSlabCache(uint32_t start) {
    Cache* cache = (Cache*)start;
    cache->size = sizeof(Cache);
    cache->slabs = NULL;
}