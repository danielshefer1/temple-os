#include "paging_bootstrap.h"

__attribute__((section(".bootstrap")))
uint32_t page_dir_addr(void) {
    uint32_t total_pages = (uint32_t)&__total_pages;
    //kprintf("Kernel Base Address: 0x%x\n", KERNEL_BASE);
    return KERNEL_BASE + (total_pages * PAGE_SIZE);
}

__attribute__((section(".bootstrap")))
void InitPageDirectory(pde_t* page_directory, uint32_t pd_addr, uint32_t kernel_pages) {
    
    uint32_t pt_addr = pd_addr;

    pt_addr += PAGE_SIZE;
    page_directory[0].present = 1;
    page_directory[0].rw = 1;
    page_directory[0].user = 0;
    page_directory[0].write_thru = 0;
    page_directory[0].cache_dis = 0;
    page_directory[0].accessed = 0;
    page_directory[0].dirty = 0;
    page_directory[0].pat = 0;
    page_directory[0].global = 0;
    page_directory[0].frame = pt_addr >> 12;

    pte_t* page_table = (pte_t*) pt_addr;
    InitPageTable(page_table, kernel_pages, false);

    pt_addr += PAGE_SIZE;
    page_directory[HIGHER_HALF_IDX].present = 1;
    page_directory[HIGHER_HALF_IDX].rw = 1;
    page_directory[HIGHER_HALF_IDX].user = 0;
    page_directory[HIGHER_HALF_IDX].write_thru = 0;
    page_directory[HIGHER_HALF_IDX].cache_dis = 0;
    page_directory[HIGHER_HALF_IDX].accessed = 0;
    page_directory[HIGHER_HALF_IDX].dirty = 0;
    page_directory[HIGHER_HALF_IDX].pat = 0;
    page_directory[HIGHER_HALF_IDX].global = 1;
    page_directory[HIGHER_HALF_IDX].frame = pt_addr >> 12;

    page_table = (pte_t*) pt_addr;
    InitPageTable(page_table, kernel_pages, true);

    for (uint32_t i = 1; i < 1024; i++) {
        if (i == HIGHER_HALF_IDX) continue;
        page_directory[i].present = 0;
    }
}

__attribute__((section(".bootstrap")))
void InitPageTable(pte_t* page_table, uint32_t kernel_pages, bool is_global) {
    uint32_t low_mem_end = 0x100000;  // 1MB
    uint32_t kernel_size = kernel_pages * PAGE_SIZE;
    uint32_t page_tables_size = 3 * PAGE_SIZE;
    uint32_t stack_size = 0x4000;  // 16KB stack

    uint32_t total_end = low_mem_end + kernel_size + page_tables_size + stack_size + PAGE_SIZE * 4;
    uint32_t entries_needed = (total_end) / 0x1000;

    for (uint32_t i = 0; i < entries_needed; i++) {
        page_table[i].present = 1;
        page_table[i].rw = 1;
        page_table[i].user = 0;
        page_table[i].write_thru = 0;
        page_table[i].cache_dis = 0;
        page_table[i].accessed = 0;
        page_table[i].dirty = 0;
        page_table[i].pat = 0;
        if (is_global)  page_table[i].global = 1;
        else page_table[i].global = 0;

        page_table[i].frame = (i * PAGE_SIZE) >> 12;
    }
    for (uint32_t i = entries_needed; i < 1024; i++) {
        page_table[i].present = 0;
    }
}