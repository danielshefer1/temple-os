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
    uint32_t next_table = curr_table + 1;
    pd[next_table].present = 1;
    pd[next_table].rw = 1;
    pd[next_table].user = 0;
    pd[next_table].write_thru = 0;
    pd[next_table].cache_dis = 0;
    pd[next_table].accessed = 0;
    pd[next_table].dirty = 0;
    pd[next_table].pat = 0;
    pd[next_table].global = 1;
    pd[next_table].frame = (curr_table * TABLE_SIZE + curr_page * PAGE_SIZE) >> 12;
    
    curr_page++;
    if (curr_page == 1024) {
        curr_page = 0;
        curr_table = next_table;
    }
    return KERNEL_VIRTUAL + curr_table * TABLE_SIZE;
}

uint32_t AddKernelPages(uint32_t num_pages) {
    if (curr_page + num_pages >= 1023) AddKernelPageTable();
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