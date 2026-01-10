#include "slab_alloc.h"

static Cache caches[NUM_CACHE];
static uint32_t sizes[NUM_CACHE] = {sizeof(BuddyNode), PAGE_SIZE};
static uint32_t slab_sizes[NUM_CACHE] = {1, 16};
static uint32_t curr_addr;

void InitSlabAlloc(uint32_t start) {
    uint32_t start_addr;
    curr_addr = start;
    

    for (uint32_t i = 0; i < NUM_CACHE; i++) {
        caches[i].size = sizes[i];
        start_addr = AddKernelPages(slab_sizes[i]);
        caches[i].empty_slabs = (Slab*) curr_addr;
        caches[i].empty_slabs->start = (void*) start_addr;
        caches[i].empty_slabs->num_slots = slab_sizes[i] * PAGE_SIZE / sizes[i];
        caches[i].empty_slabs->free_count = slab_sizes[i] * PAGE_SIZE / sizes[i];
        caches[i].empty_slabs->bitmap_size = (slab_sizes[i] * PAGE_SIZE / sizes[i]) / 32;
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
    uint32_t slab_size = slab_sizes[cache_idx];
    void* slab_addr = (void*) AddKernelPages(slab_size);
    Slab* emps_p = cache->empty_slabs;
    
    emps_p = AddSlab((slab_sizes[cache_idx] * PAGE_SIZE / sizes[cache_idx]) / 32);
    emps_p->start = slab_addr;
    emps_p->num_slots = slab_size * PAGE_SIZE / sizes[cache_idx];
    emps_p->free_count = slab_size * PAGE_SIZE / sizes[cache_idx];
    emps_p->bitmap_size = (slab_size * PAGE_SIZE / sizes[cache_idx]) / 32;

    for (uint32_t i = 0; i < emps_p->bitmap_size; i++) {
        emps_p->bitmap[i] = 0;
    }
}

Slab* SearchSlab(Slab* slab1, Slab* slab2, void* ptr, uint32_t cache_idx) {
    Slab* p1 = slab1, *p2 = slab2;
    while (p1 != NULL && p2 != NULL) {
        if (p1->start <= ptr && p1->start + slab_sizes[cache_idx] * PAGE_SIZE > ptr) {
            return p1;
        }
        if (p2->start <= ptr && p2->start + slab_sizes[cache_idx] * PAGE_SIZE > ptr) {
            return p2;
        }
        p1 = p1->next;
        p2 = p2->next;
    }
    while (p1 != NULL) {
        if (p1->start <= ptr && p1->start + slab_sizes[cache_idx] * PAGE_SIZE > ptr) {
            return p1;
        }
        p1 = p1->next;
    }
    while (p2 != NULL) {
        if (p2->start <= ptr && p2->start + slab_sizes[cache_idx] * PAGE_SIZE > ptr) {
            return p2;
        }
        p2 = p2->next;
    }
    return NULL;
}

void* kmalloc(uint32_t size) {
    void* ret;

    for (uint32_t i = 0; i < NUM_CACHE; i++) {
        if (caches[i].size == size) {
            ret = SearchCache(&caches[i], i);
            if (ret != NULL) {
                memset(ret, 0, size);
                return ret;
            }
            AddSlabW(&caches[i], i);
            ret = SearchCache(&caches[i], i);
            memset(ret, 0, size);
            return ret;            
        }
    }
    return NULL;
}

void kfree(void* ptr, uint32_t size) {
    uint32_t slot_index, bitmap_index, bit_pos;

    memset(ptr, SLAB_GARBAGE_BYTE, size);

    for (uint32_t i = 0; i < NUM_CACHE; i++) {
        if (caches[i].size == size) {
            Slab* p = SearchSlab(caches[i].full_slabs, caches[i].partial_slabs, ptr, i);
            if (p == NULL) {
                return;
            }
            p->free_count++;
            slot_index = (uint32_t) ptr - (uint32_t) p->start;
            bitmap_index = slot_index / 32;
            bit_pos = slot_index % 32;
            p->bitmap[bitmap_index] = p->bitmap[bitmap_index] ^ (1 << bit_pos);            
            if (p->free_count == 1) {
                caches[i].full_slabs = p->next;
                p->next = caches[i].partial_slabs;
                caches[i].partial_slabs = p;
            }
            if (p->free_count == p->num_slots) {
                caches[i].partial_slabs = p->next;
                p->next = caches[i].empty_slabs;
                caches[i].empty_slabs = p;
            }
            return;
        }
    }
}


void memset(void* address, uint8_t value, uint32_t size) {
    uint32_t reminder = size % 4;
    size -= reminder;
    size /= 4;
    uint32_t value_32 = value + (value << 8) + (value << 16) + (value << 24);

    for(uint32_t i = 0; i < size; i++) {
        ((uint32_t*) address)[i] = value_32;
    }

    for(uint32_t i = 0; i < reminder; i++) {
        ((uint8_t*) address)[size * 4 +i] = value;
    }
}