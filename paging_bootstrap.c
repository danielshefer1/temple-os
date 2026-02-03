#include "bootstrap.h"

page_entry_t pml4[512] __attribute__((aligned(4096)));
page_entry_t pdpt[512] __attribute__((aligned(4096)));
page_entry_t pd[512] __attribute__((aligned(4096)));

void InitPaging() {
    uint32_t kernel_pages = ((uint32_t)&(kernel_sectors)) * PAGE_SIZE / 512;
    uint32_t kernel_big_pages = (kernel_pages + TABLE_SIZE - 1) / TABLE_SIZE;

    uint64_t pml4_idx = PML4_IDX(KERNEL_VIRTUAL);
    uint64_t pdpt_idx = PDPT_IDX(KERNEL_VIRTUAL);

    // Start PML4 Mapping
    pml4[0].present = 1;
    pml4[0].writable = 1;
    pml4[0].address = (uint64_t)pdpt >> 12;

    pml4[pml4_idx].present = 1;
    pml4[pml4_idx].writable = 1;
    pml4[pml4_idx].address = (uint64_t)pdpt >> 12;
    // End PML4 Mapping

    // Start PDPT Mapping
    pdpt[0].present = 1;
    pdpt[0].writable = 1;
    pdpt[0].address = (uint64_t)pd >> 12;

    pdpt[pdpt_idx].present = 1;
    pdpt[pdpt_idx].writable = 1;
    pdpt[pdpt_idx].address = (uint64_t)pd >> 12;
    // Start PDPT Mapping

    // Start PD Mapping
    pd[0].present = 1;
    pd[0].page_size = 1;
    pd[0].writable = 1;
    pd[0].pcd = 1;
    pd[0].address = 0;

    uint32_t base_idx = PD_IDX(KERNEL_VIRTUAL);
    for (uint32_t i = 0; i < kernel_big_pages; i++) {
        uint32_t idx = base_idx + i;

        pd[idx].present = 1;
        pd[idx].page_size = 1;
        pd[idx].writable = 1;
        pd[idx].address = (2*MB * (i + 1)) >> 12;

        pd[i + 1].present = 1;
        pd[i + 1].page_size = 1;
        pd[i + 1].writable = 1;
        pd[i + 1].address = (2*MB * (i + 1)) >> 12;
    }
    // End PD Mapping    
}

page_entry_t* GetPML4() { return pml4; }