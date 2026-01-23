#include "slab_alloc.h"


static uint32_t sizes[] = {sizeof(buddy_node_t), PAGE_SIZE, sizeof(vfs_dentry_t), sizeof(vfs_inode_t), sizeof(dcache_entry_t)};
static uint32_t slab_sizes[] = {1, 32, 2, 2, 1};
static cache_t caches[sizeof(sizes) / sizeof(sizes[0])];
static uint32_t curr_addr;
static uint32_t num_cache = sizeof(sizes) / sizeof(sizes[0]);

void InitSlabAlloc(uint32_t start) {
    uint32_t start_addr;
    curr_addr = start;
    
    if (sizeof(sizes) != sizeof(slab_sizes)) {
        kerror("sizes, slab_sizes don't line up!");
    }

    for (uint32_t i = 0; i < num_cache; i++) {
        caches[i].size = sizes[i];
        start_addr = AddKernelPages(slab_sizes[i]);
        caches[i].empty_slabs = (slab_t*) curr_addr;
        caches[i].empty_slabs->start = (void*) start_addr;
        caches[i].empty_slabs->num_slots = slab_sizes[i] * PAGE_SIZE / sizes[i];
        caches[i].empty_slabs->free_count = slab_sizes[i] * PAGE_SIZE / sizes[i];
        caches[i].empty_slabs->bitmap_size = CalculateBitMapSize(i);
        for (uint32_t j = 0; j < caches[i].empty_slabs->bitmap_size; j++) {
            caches[i].empty_slabs->bitmap[j] = 0;
        }
        curr_addr += sizeof(slab_t) + sizeof(uint32_t) * caches[i].empty_slabs->bitmap_size;
    }
}

uint32_t CalculateBitMapSize(uint32_t i) {
    if ((slab_sizes[i] * PAGE_SIZE / sizes[i]) % 32 == 0) return (slab_sizes[i] * PAGE_SIZE / sizes[i]) / 32; 
    else return (slab_sizes[i] * PAGE_SIZE / sizes[i]) / 32 + 1;
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

void* SearchCache(cache_t* cache, uint32_t cache_idx) {
    slab_t* s_p;

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
                cache->partial_slabs = DeleteSlab(cache->partial_slabs, s_p);
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
        s_p->free_count--;
        cache->empty_slabs = s_p->next;
        s_p->next = cache->partial_slabs;
        cache->partial_slabs = s_p;
        return ret;
    }
    return NULL;
}

slab_t* AddSlab(uint32_t bitmap_size) {
    curr_addr += sizeof(slab_t) + sizeof(uint32_t) * bitmap_size;
    return (slab_t*) (curr_addr - sizeof(slab_t) - sizeof(uint32_t) * bitmap_size);
}

void AddSlabW(cache_t* cache, uint32_t cache_idx) {
    uint32_t slab_size = slab_sizes[cache_idx];
    void* slab_addr = (void*) AddKernelPages(slab_size);
    
    // 1. Calculate how many bitmap uint32s we need
    uint32_t total_slots = slab_size * PAGE_SIZE / sizes[cache_idx];
    uint32_t bitmap_len = total_slots / 32;
    if (total_slots % 32 != 0) bitmap_len++; // Safety for non-aligned sizes

    // 2. Allocate the metadata structure
    slab_t* new_slab = AddSlab(bitmap_len);
    
    // 3. Initialize metadata
    new_slab->start = slab_addr;
    new_slab->num_slots = total_slots;
    new_slab->free_count = total_slots;
    new_slab->bitmap_size = bitmap_len;

    for (uint32_t i = 0; i < new_slab->bitmap_size; i++) {
        new_slab->bitmap[i] = 0;
    }

    // 4. IMPORTANT: Link the new slab to the cache!
    new_slab->next = cache->empty_slabs;
    cache->empty_slabs = new_slab;
}

slab_t* SearchSlab(slab_t* slab1, slab_t* slab2, void* ptr, uint32_t cache_idx) {
    slab_t* p1 = slab1, *p2 = slab2;
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

// Returns the index of the smallest cache that can fit 'size'
// Returns -1 if the requested size is too large for any cache
uint32_t GetBestCacheIndex(uint32_t size) {
    uint32_t best_idx = 0xFFFFFFF;
    uint32_t min_waste = 0xFFFFFFFF;

    for (uint32_t i = 0; i < num_cache; i++) {
        if (caches[i].size >= size) {
            uint32_t waste = caches[i].size - size;
            
            // Found a better fit (smaller waste)
            if (waste < min_waste) {
                min_waste = waste;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

void* kmalloc(uint32_t size) {

    int idx = GetBestCacheIndex(size);
    
    // If size is too huge (larger than PAGE_SIZE cache), return NULL
    if (idx == -1) {
        return NULL; 
    }
    bool org_int_state = check_interrupts();
    CliHelper();
    // Try to find a slot in the existing slabs
    void* ret = SearchCache(&caches[idx], idx);
    if (ret != NULL) {
        memset(ret, 0, size);
        if (org_int_state) StiHelper();
        return ret;
    }

    // No slot found, allocate a new Slab
    AddSlabW(&caches[idx], idx);
    
    // Search again in the newly added slab
    ret = SearchCache(&caches[idx], idx);
    if (ret != NULL) {
        memset(ret, 0, size);
    }
    if (org_int_state) StiHelper();
    return ret;
}

slab_t* DeleteSlab(slab_t* head, slab_t* target) {
    if (head == NULL) return head;
    if (head == target) return head->next;
    slab_t* p = head;

    while (p->next != NULL && p->next != target) {
        p = p->next;
    }
    if (p->next == NULL) return head;

    p->next = p->next->next;
    return head;
}

void kfree(void* ptr, uint32_t size) {
    
    uint32_t slot_index, bitmap_index, bit_pos;
    memset(ptr, SLAB_GARBAGE_BYTE, size);

    uint32_t idx = GetBestCacheIndex(size);
    if (idx == 0xFFFFFFF) {
        return;
    } 

    cache_t* cache = &caches[idx];

    slab_t* p = SearchSlab(cache->full_slabs, cache->partial_slabs, ptr, idx);
    
    if (p == NULL) {
        return;
    }

    bool org_int_state = check_interrupts();
    CliHelper();
    
    p->free_count++;
    slot_index = (uint32_t) ptr - (uint32_t) p->start;
    bitmap_index = slot_index / 32;
    bit_pos = slot_index % 32;

    p->bitmap[bitmap_index] ^=  (1 << bit_pos);        

    if (p->free_count == 1) {
        caches[idx].full_slabs = DeleteSlab(caches[idx].full_slabs, p);
        p->next = caches[idx].partial_slabs;
        caches[idx].partial_slabs = p;
    }
    if (p->free_count == p->num_slots) {
        caches[idx].partial_slabs = DeleteSlab(caches[idx].partial_slabs, p);
        p->next = caches[idx].empty_slabs;
        caches[idx].empty_slabs = p;
    }
    if (org_int_state) StiHelper();
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