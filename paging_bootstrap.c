#include "bootstrap.h"

page_entry_t pml4[512] __attribute__((aligned(4096)));

page_entry_t identity_pdpt[512] __attribute__((aligned(4096)));
page_entry_t identity_pd[512] __attribute__((aligned(4096)));

page_entry_t kernel_pdpt[512] __attribute__((aligned(4096)));
page_entry_t kernel_pd[512] __attribute__((aligned(4096)));

void InitPaging() {
    volatile uint32_t* stage4 = (volatile uint32_t*)  0x8E00;
    uint32_t kernel_bss_end = stage4[4];
    uint32_t kernel_size = kernel_bss_end - KERNEL_BASE;
    uint32_t kernel_big_pages = (kernel_size + TABLE_SIZE - 1) / TABLE_SIZE;

    uint64_t kernel_pml4_idx = PML4_IDX(KERNEL_VIRTUAL);
    uint64_t kernel_pdpt_idx = PDPT_IDX(KERNEL_VIRTUAL);
    uint64_t base_idx = PD_IDX(KERNEL_VIRTUAL);

    uint64_t identity_pml4_idx = PML4_IDX(KERNEL_BASE);
    uint64_t identity_pdpt_idx = PDPT_IDX(KERNEL_BASE);
    uint64_t identity_pd_idx = PD_IDX(KERNEL_BASE) - 1;

    // Start PML4 Mapping
    pml4[identity_pml4_idx].present = 1;
    pml4[identity_pml4_idx].writable = 1;
    pml4[identity_pml4_idx].address = (uint32_t)identity_pdpt >> 12;

    pml4[kernel_pml4_idx].present = 1;
    pml4[kernel_pml4_idx].writable = 1;
    pml4[kernel_pml4_idx].address = (uint32_t)kernel_pdpt >> 12;
    // End PML4 Mapping

    // Start PDPT Mapping
    identity_pdpt[identity_pdpt_idx].present = 1;
    identity_pdpt[identity_pdpt_idx].writable = 1;
    identity_pdpt[identity_pdpt_idx].address = (uint32_t)identity_pd >> 12;

    kernel_pdpt[kernel_pdpt_idx].present = 1;
    kernel_pdpt[kernel_pdpt_idx].writable = 1;
    kernel_pdpt[kernel_pdpt_idx].address = (uint32_t)kernel_pd >> 12;
    // Start PDPT Mapping

    // Start PD Mapping
    
    for (uint32_t i = 0; i < kernel_big_pages + 1; i++) {
        uint32_t idx = base_idx + i;

        if (i == 0) {
            kernel_pd[idx].pcd = 1;
            identity_pd[identity_pd_idx + i].pcd = 1;
        }

        kernel_pd[idx].present = 1;
        kernel_pd[idx].page_size = 1;
        kernel_pd[idx].writable = 1;
        kernel_pd[idx].address = (2*MB * i) >> 12;

        identity_pd[identity_pd_idx + i].present = 1;
        identity_pd[identity_pd_idx + i].page_size = 1;
        identity_pd[identity_pd_idx + i].writable = 1;
        identity_pd[identity_pd_idx + i].address = (2*MB * i) >> 12;
    }
    // End PD Mapping    
}

page_entry_t* GetPML4() { return pml4; }