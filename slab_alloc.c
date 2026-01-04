#include "slab_alloc.h"

static Cache caches[NUM_CACHE];
static uint32_t sizes[2] = {sizeof(BuddyList), sizeof(BuddyNode)};
static uint32_t size_slabs[2] = {1, 1};
static uint32_t curr_addr;

void InitSlabCache(uint32_t start) {
    uint32_t start_addr;
    curr_addr = start;

    for (uint32_t i = 0; i < NUM_CACHE; i++) {
        caches[i].size = sizes[i];
        start_addr = AddKernelPages(size_slabs[i]);
        caches[i].empty_slabs = (Slab*) curr_addr;
        caches[i].empty_slabs->start = (void*) start_addr;
        caches[i].empty_slabs->num_slots = size_slabs[i] * PAGE_SIZE / sizes[i];
        caches[i].empty_slabs->free_count = size_slabs[i] * PAGE_SIZE / sizes[i];
        caches[i].empty_slabs->bitmap_size = (size_slabs[i] * PAGE_SIZE / sizes[i]) / 32;
        for (uint32_t j = 0; j < caches[i].empty_slabs->bitmap_size; j++) {
            caches[i].empty_slabs->bitmap[j] = 0;
        }
        curr_addr += sizeof(Slab) + sizeof(uint32_t) * caches[i].empty_slabs->bitmap_size;
    }
}
uint32_t GetEmptyBit(uint32_t num) {
    uint32_t bit_pos = 0, bit;
    while (bit_pos < 32) {
        bit = num << (31 - bit_pos);
        bit = bit >> 31;
        if (bit == 0) {
            break;
        }
        bit_pos++;
    } 
    return bit_pos;
}

void* SearchCache(Cache* cache, uint32_t cache_idx) {
    Slab* s_p;

    if (cache->partial_slabs != NULL) {
        s_p = cache->partial_slabs;
        for (uint32_t i = 0; i < s_p->bitmap_size; i++) {
            if (s_p->bitmap[i] == 0xFFFFFFFF) {
                continue;
            }
            uint32_t bit_pos = GetEmptyBit(s_p->bitmap[i]);
            s_p->bitmap[i] = s_p->bitmap[i] ^ (1 << bit_pos);
            s_p->free_count--;
            if (s_p->free_count == 0) {
                cache->partial_slabs = s_p->next;
                s_p->next = cache->full_slabs;
                cache->full_slabs = s_p;
            }
            return (void*) ((uint32_t) s_p->start + ((i * 32) + bit_pos) * sizes[cache_idx]);
        }
        s_p = s_p->next;
    }
    if (cache->empty_slabs != NULL) {
        s_p = cache->empty_slabs;
        void* ret = (void*) s_p->start;
        s_p->bitmap[0] = 1;
        cache->empty_slabs = s_p->next;
        s_p->next = cache->partial_slabs;
        cache->partial_slabs = s_p;
        return ret;
    }
    return NULL;
}

Slab* AddSlab(uint32_t bitmap_size) {
    curr_addr += sizeof(Slab) + sizeof(uint32_t) * bitmap_size;
    return (Slab*) (curr_addr - sizeof(Slab) - sizeof(uint32_t) * bitmap_size);
}

void AddSlabW(Cache* cache, uint32_t cache_idx) {
    uint32_t slab_size = size_slabs[cache_idx];
    void* slab_addr = (void*) AddKernelPages(slab_size);
    Slab* emps_p = cache->empty_slabs;
    
    emps_p = AddSlab((size_slabs[cache_idx] * PAGE_SIZE / sizes[cache_idx]) / 32);
    emps_p->start = slab_addr;
    emps_p->num_slots = slab_size * PAGE_SIZE / sizes[cache_idx];
    emps_p->free_count = slab_size * PAGE_SIZE / sizes[cache_idx];
    emps_p->bitmap_size = (slab_size * PAGE_SIZE / sizes[cache_idx]) / 32;

    for (uint32_t i = 0; i < emps_p->bitmap_size; i++) {
        emps_p->bitmap[i] = 0;
    }
}

void* kmalloc(uint32_t size) {
    void* ret;

    for (uint32_t i = 0; i < NUM_CACHE; i++) {
        if (caches[i].size == size) {
            ret = SearchCache(&caches[i], i);
            if (ret != NULL) {
                return ret;
            }
            AddSlabW(&caches[i], i);
            ret = SearchCache(&caches[i], i);
            return ret;            
        }
    }
    return NULL;
}

void kfree(void* ptr, uint32_t size) {
    uint32_t slot_index, bitmap_index, bit_pos;

    for (uint32_t i = 0; i < NUM_CACHE; i++) {
        if (caches[i].size == size) {
            Slab* s_p = caches[i].full_slabs;
            while (!(s_p->start <= ptr &&
                 s_p->start + size_slabs[i] * PAGE_SIZE > ptr)) {
                s_p = s_p->next;
            }
            s_p->free_count++;
            slot_index = (uint32_t) ptr - (uint32_t) s_p->start;
            bitmap_index = slot_index / 32;
            bit_pos = slot_index % 32;
            s_p->bitmap[bitmap_index] = s_p->bitmap[bitmap_index] ^ (1 << bit_pos);            
            if (s_p->free_count == s_p->num_slots) {
                caches[i].full_slabs = s_p->next;
                s_p->next = caches[i].empty_slabs;
                caches[i].empty_slabs = s_p;
            }
            if (s_p->free_count == 1) {
                caches[i].full_slabs = s_p->next;
                s_p->next = caches[i].partial_slabs;
                caches[i].partial_slabs = s_p;
            }
        }
    }
}
