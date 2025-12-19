#include "paging.h"

// Define the constant here
uint32_t KERNEL_BASE = (uint32_t)&(_kernel_VMA_start);

uint32_t num_usable_entries(E820Info* info) {
    uint32_t count = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type != 1) {
            continue;
        }
        if (info->entries[i].length_low < 1024 * 4 && info->entries[i].length_high != 0) {
            continue;
        }
        if (info->entries[i].base_high != 0 || info->entries[i].base_low == 0) {
            continue;
        }
        count++;
    }
    return count;
}

E820Entry* fetch_usable_memory(E820Info* info, E820Entry* usable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type != 1) {
            continue;
        }
        if (info->entries[i].length_low < 1024 * 4 && info->entries[i].length_high != 0) {
            continue;
        }
        if (info->entries[i].base_high != 0 || info->entries[i].base_low == 0) {
            continue;
        }
        usable_entries[index++] = info->entries[i];
    }
    return usable_entries;
}

uint32_t page_dir_addr(void) {
    uint32_t total_pages = (uint32_t)&__total_pages;
    return KERNEL_BASE + (total_pages * PAGE_SIZE);
}

uint32_t fill_page_table(pte_t* page_table, const E820Entry* usable_memory,
     const uint32_t kernel_pages, const uint16_t idx) {
    Tuple range;
    range.first = usable_memory->base_low;
    range.second = usable_memory->base_low + usable_memory->length_low;
    
    for (size_t i = 0; i < 1024; i++) {
        const uint32_t curr_paddr = i * PAGE_SIZE;
        
        if (curr_paddr < range.first || curr_paddr >= range.second) {
            page_table[i].present = 1;
            page_table[i].rw = 0;
            page_table[i].user = 0;
            page_table[i].global = 0;
            page_table[i].cache_dis = 0;
            page_table[i].accessed = 0;
            page_table[i].pat = 0;
            page_table[i].dirty = 0;
            page_table[i].frame = curr_paddr >> 12;
            continue;
        }
        
        bool kernel_identy_map = curr_paddr >= KERNEL_BASE && 
                                  curr_paddr < (KERNEL_BASE + (kernel_pages * PAGE_SIZE));
        bool kernel_higher_half = curr_paddr >= KERNEL_VIRTUAL && 
                                   curr_paddr < (KERNEL_VIRTUAL + (kernel_pages * PAGE_SIZE));
        
        if (kernel_identy_map || kernel_higher_half) {
            page_table[i].present = 1;
            page_table[i].rw = 1;
            page_table[i].user = 0;
            page_table[i].frame = curr_paddr >> 12;
            page_table[i].avail = 0;
            page_table[i].global = 1;
            page_table[i].write_thru = 0;
            page_table[i].cache_dis = 0;
            page_table[i].accessed = 0;
            page_table[i].dirty = 0;
            page_table[i].pat = 0;
            continue;
        }
        
        page_table[i].present = 0;
    }
}