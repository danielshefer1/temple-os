#include "paging.h"

__attribute__((section(".bootstrap")))
uint32_t page_dir_addr(void) {
    uint32_t total_pages = (uint32_t)&__total_pages;
    kprintf("Kernel Base Address: 0x%x\n", KERNEL_BASE);
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

bool InitPageTableBasedMP(uint32_t idx, E820Entry* unusable, uint8_t unusable_length) {
    for (size_t i = 0; i < unusable_length; i++) {
        if (isOverlapping(i * TABLE_SIZE, i * TABLE_SIZE + TABLE_SIZE,
             unusable[i].base_low, unusable[i].base_low + unusable[i].length_low)) {
            return true;
        }
    }
    return false;
}

bool isOverlapping(uint32_t startA, uint32_t endA, uint32_t startB, uint32_t endB) {
    return startA <= endB && startB <= endA;
}