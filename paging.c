#include "paging.h"

static pde_t* pd;
static pte_t* pt;
static uint32_t curr_page;
static uint32_t curr_table;

void flush_tlb() {
    __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");
}
void InitPaging() {
    uint32_t pd_addr = PageDirAddrV();
    pd = (pde_t*) pd_addr;
    pt = (pte_t*)(KERNEL_VIRTUAL + (pd[HIGHER_HALF_IDX].frame << 12));
    pd[0].present = 0;
    flush_tlb();
    uint32_t idx = 0;
    while (pt[idx].present == 1) {
        idx++;
    }
    curr_page = idx;
    curr_table = 0;
    AddGuardPage(HIGHER_HALF_IDX, curr_page - 8);
}
uint32_t PageDirAddrV() {
    uint32_t kernel_pages = (uint32_t)&__total_pages;
    return KERNEL_VIRTUAL + KERNEL_BASE + kernel_pages * PAGE_SIZE;
}

uint32_t AddKernelPageTable() {
    uint32_t next_pt = (uint32_t) kmalloc(sizeof(pte_t) * 1024);
    curr_table++;
    pd[curr_table].present = 1;
    pd[curr_table].rw = 1;
    pd[curr_table].user = 0;
    pd[curr_table].write_thru = 0;
    pd[curr_table].cache_dis = 0;
    pd[curr_table].accessed = 0;
    pd[curr_table].dirty = 0;
    pd[curr_table].pat = 0;
    pd[curr_table].global = 1;
    pd[curr_table].frame = (next_pt - KERNEL_VIRTUAL) >> 12;
    

    return KERNEL_VIRTUAL + curr_table * TABLE_SIZE;
}
uint32_t AddUserPageTable(uint32_t table_idx) {
    if (pd[table_idx].present == 1) {
        return KERNEL_VIRTUAL + table_idx * TABLE_SIZE;
    }
    uint32_t pt_addr = (uint32_t) kmalloc(sizeof(pte_t) * 1024);
    pd[table_idx].present = 1;
    pd[table_idx].rw = 1;
    pd[table_idx].user = 1;
    pd[table_idx].write_thru = 0;
    pd[table_idx].cache_dis = 0;
    pd[table_idx].accessed = 0;
    pd[table_idx].dirty = 0;
    pd[table_idx].pat = 0;
    pd[table_idx].global = 0;
    pd[table_idx].frame = (pt_addr - KERNEL_VIRTUAL) >> 12;

    return KERNEL_VIRTUAL + curr_table * TABLE_SIZE;
}

void FillUserPageTable(uint32_t table_idx, uint32_t start_page, uint32_t num_pages) {
    pte_t* user_pt = (pte_t*)(KERNEL_VIRTUAL + (pd[table_idx].frame << 12));
    uint32_t idx = start_page, end = start_page + num_pages;

    for (; idx < end; idx++) {
        user_pt[idx].present = 1;
        user_pt[idx].rw = 1;
        user_pt[idx].user = 1;
        user_pt[idx].write_thru = 0;
        user_pt[idx].cache_dis = 0;
        user_pt[idx].accessed = 0;
        user_pt[idx].dirty = 0;
        user_pt[idx].pat = 0;
        user_pt[idx].global = 0;
        user_pt[idx].frame = (table_idx * TABLE_SIZE + idx * PAGE_SIZE + (KERNEL_VIRTUAL >> 1)) >> 12;
    }
}

void FillPageDirectory(void* addr, uint32_t size) {

    uint32_t current_addr = (uint32_t)addr;
    uint32_t end_addr = current_addr + size;

    while (current_addr < end_addr) {
        // 1. Calculate the Page Directory Index
        // Standard x86: Shift right by 22 to get the top 10 bits
        uint32_t pd_index = (current_addr - KERNEL_VIRTUAL / 2) >> 22; 
        
        // 2. Calculate the Page Table Index (0 to 1023)
        // Shift right by 12, then mask the bottom 10 bits
        uint32_t pt_index = (current_addr >> 12) & 0x3FF;

        // 3. Calculate how many pages fit in THIS specific table
        // The table ends at entry 1024. 
        uint32_t pages_left_in_table = 1024 - pt_index;

        // 4. Calculate how many pages we actually need to map right now
        // It's the minimum of: what fits in the table vs. what we have left to map
        uint32_t bytes_remaining = end_addr - current_addr;
        uint32_t pages_remaining = (bytes_remaining + PAGE_SIZE - 1) / PAGE_SIZE; 
        
        uint32_t pages_to_fill = (pages_remaining < pages_left_in_table) 
                                 ? pages_remaining 
                                 : pages_left_in_table;

        // 5. Perform the mapping
        AddUserPageTable(pd_index); // Ensure the table exists
        FillUserPageTable(pd_index, pt_index, pages_to_fill);

        // 6. Advance current_addr by the amount we just mapped
        current_addr += pages_to_fill * PAGE_SIZE;
    }
}

void RemovePageTables(uint32_t start_table, uint32_t end_table) {
    for (uint32_t t = start_table; t < end_table; t++) {
        if (pd[t].present == 1) {
            kfree((void*)(KERNEL_VIRTUAL + (pd[t].frame << 12)), sizeof(pte_t) * 1024);
        }
    }
    flush_tlb();
}

void RemovePages(uint32_t table_idx, uint32_t start_page, uint32_t num_pages) {
    pte_t* user_pt = (pte_t*)(KERNEL_VIRTUAL + (pd[table_idx].frame << 12));
    uint32_t idx = start_page, end = start_page + num_pages;

    for (; idx < end; idx++) {
        user_pt[idx].present = 0;
    }
    flush_tlb();
}

uint32_t AddKernelPages(uint32_t num_pages) {
    if (curr_page + num_pages >= 1024) AddKernelPageTable();
    uint32_t idx = curr_page, ret = KERNEL_VIRTUAL + idx * PAGE_SIZE + curr_table * TABLE_SIZE, end = idx + num_pages;

    for(; idx < end; idx++) {
        if (curr_page == 1024) {
            curr_page = 0;
            curr_table++;
            pt = (pte_t*)(KERNEL_VIRTUAL + (pd[curr_table].frame << 12));
        }
        pt[curr_page].present = 1;
        pt[curr_page].rw = 1;
        pt[curr_page].user = 0;
        pt[curr_page].write_thru = 0;
        pt[curr_page].cache_dis = 0;
        pt[curr_page].accessed = 0;
        pt[curr_page].dirty = 0;
        pt[curr_page].pat = 0;
        pt[curr_page].global = 1;
        pt[curr_page].frame = (curr_table * TABLE_SIZE + idx * PAGE_SIZE) >> 12;
        curr_page++;
    }
    return ret;
}

void AddGuardPage(uint32_t Tidx, uint32_t Pidx) {
    if (pd[Tidx].present == 0) {
        return;
    }
    pte_t* page_table = (pte_t*)(KERNEL_VIRTUAL + (pd[Tidx].frame << 12));
    page_table[Pidx].present = 0;
}