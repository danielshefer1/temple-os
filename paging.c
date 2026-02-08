#include "paging.h"

page_entry_t pml4[512] __attribute__((aligned(4096)));

page_entry_t kernel_pdpt[512] __attribute__((aligned(4096)));
page_entry_t kernel_pd[512] __attribute__((aligned(4096)));

static uint64_t curr_addr, curr_addr;

void InitPaging() {
    uint64_t kernel_size = (uint64_t)&__kernel_size_bytes;
    uint64_t text_size = (uint64_t)&__text_size;

    uint64_t kernel_big_pages = (kernel_size + TABLE_SIZE - 1) / TABLE_SIZE;
    uint64_t text_big_pages = (text_size + TABLE_SIZE - 1) / TABLE_SIZE;

    curr_addr = KERNEL_VIRTUAL + kernel_big_pages * TABLE_SIZE;

    uint64_t kernel_pml4_idx = PML4_IDX(KERNEL_VIRTUAL);
    uint64_t kernel_pdpt_idx = PDPT_IDX(KERNEL_VIRTUAL);
    uint64_t base_idx = PD_IDX(KERNEL_VIRTUAL);


    // Start PML4 Mapping
    pml4[kernel_pml4_idx].present = 1;
    pml4[kernel_pml4_idx].writable = 1;
    pml4[kernel_pml4_idx].address = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)kernel_pdpt) >> 12;
    // End PML4 Mapping

    // Start PDPT Mapping
    kernel_pdpt[kernel_pdpt_idx].present = 1;
    kernel_pdpt[kernel_pdpt_idx].writable = 1;
    kernel_pdpt[kernel_pdpt_idx].address = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)kernel_pd) >> 12;
    // Start PDPT Mapping

    // Start PD Mapping
    for (uint64_t i = 0; i < kernel_big_pages + 1; i++) {
        uint64_t idx = base_idx + i;

        if (i == 0) {
            kernel_pd[idx].pcd = 1;
        }
        if (i > text_big_pages) {
            kernel_pd[idx].no_execute = 1;
        }

        kernel_pd[idx].present = 1;
        kernel_pd[idx].page_size = 1;
        kernel_pd[idx].writable = 1;
        kernel_pd[idx].address = (2*MB * i) >> 12;
        kernel_pd[idx].global = 1;
    }
    // End PD Mapping    
    switch_pml4((page_entry_t*)KERNEL_VIRT_TO_PHYS((uint64_t)pml4));
}

uint64_t PageDirAddrV() {
    return (uint64_t)pml4;
}

void map_page_to_virt(uint64_t virt, uint64_t phy, uint64_t flags, bool big_page) {
    uint64_t pml4_idx = PML4_IDX(virt);
    uint64_t pdpt_idx = PDPT_IDX(virt);
    uint64_t pd_idx = PD_IDX(virt);
    uint64_t pt_idx = PT_IDX(virt);

    page_entry_t* new_pdpt, *new_pd;

    if (pml4[pml4_idx].present == 0) {
        new_pdpt = (page_entry_t*) kmalloc(PAGE_SIZE);
        memset(new_pdpt, 0, PAGE_SIZE);
        pml4[pml4_idx].present = 1;
        pml4[pml4_idx].writable = 1;
        pml4[pml4_idx].address = KERNEL_VIRT_TO_PHYS((uint64_t)new_pdpt) >> 12;
    }
    page_entry_t* pdpt = (page_entry_t*) ((pml4[pml4_idx].address << 12) + KERNEL_VIRTUAL);
    if (pdpt[pdpt_idx].present == 0) {
        new_pd = (page_entry_t*) kmalloc(PAGE_SIZE);
        memset(new_pd, 0, PAGE_SIZE);
        pdpt[pdpt_idx].present = 1;
        pdpt[pdpt_idx].writable = 1;
        pdpt[pdpt_idx].address = KERNEL_VIRT_TO_PHYS((uint64_t)new_pd) >> 12;
    }
    page_entry_t* pd = (page_entry_t*) ((pdpt[pdpt_idx].address << 12) + KERNEL_VIRTUAL);

    if (big_page) {
        if (new_pd == NULL) {
            kprintf("Tried to map a big page but page directory already exists!");
            return;
        }
        if (pt_idx != 0 && PT_IDX(phy) != 0) {
            kprintf("Tried to map a big page but not 2MB aligned!");
            return;
        }
        pd[pd_idx].present = 1;
        pd[pd_idx].writable = (flags & RW_PAGE_BIT) ? 1 : 0;
        pd[pd_idx].page_size = 1;
        pd[pd_idx].user = (flags & USER_PAGE_BIT) ? 1 : 0;
        pd[pd_idx].address = phy >> 12;
        pd[pd_idx].no_execute = (flags & NX_PAGE_BIT) ? 1 : 0;
        pd[pd_idx].pcd = (flags & CACHE_DIS_PAGE_BIT) ? 1 : 0;
        pd[pd_idx].pwt = (flags & WRITE_THROUGH_PAGE_BIT) ? 1 : 0;
        pd[pd_idx].global = (flags & GLOBAL_PAGE_BIT) ? 1 : 0;
        pd[pd_idx].page_size = 1;
        return;
    }


    if (pd[pd_idx].present == 0) {
        page_entry_t* new_pt = (page_entry_t*) kmalloc(PAGE_SIZE);
        memset(new_pt, 0, PAGE_SIZE);
        pd[pd_idx].present = 1;
        pd[pd_idx].writable = 1;
        pd[pd_idx].address = KERNEL_VIRT_TO_PHYS((uint64_t)new_pt) >> 12;
    }

    if (pd[pd_idx].page_size == 1) {
        kprintf("Tried to map a small page but a big page already exists at that address!");
        return;
    }

    page_entry_t* pt = (page_entry_t*) ((uint64_t)(pd[pd_idx].address) << 12 + KERNEL_VIRTUAL);

    pt[pt_idx].present = 1;
    pt[pt_idx].writable = (flags & RW_PAGE_BIT) ? 1 : 0;
    pt[pt_idx].user = (flags & USER_PAGE_BIT) ? 1 : 0;
    pt[pt_idx].address = phy >> 12;
    pt[pt_idx].no_execute = (flags & NX_PAGE_BIT) ? 1 : 0;
    pt[pt_idx].pcd = (flags & CACHE_DIS_PAGE_BIT) ? 1 : 0;
    pt[pt_idx].pwt = (flags & WRITE_THROUGH_PAGE_BIT) ? 1 : 0;
    pt[pt_idx].global = (flags & GLOBAL_PAGE_BIT) ? 1 : 0;
}


void FillPageDirectoryGeneral(void* virt, void* phy, uint64_t size, uint64_t flags) {
    uint64_t start_addr = (uint64_t)virt;
    uint64_t addr = (uint64_t)phy;
    uint64_t num_big_pages = size / TABLE_SIZE;
    uint64_t remaining_size = size % TABLE_SIZE;
    uint64_t num_small_pages = 0;
    if (remaining_size != 0) {
        num_small_pages = (remaining_size + PAGE_SIZE - 1) / PAGE_SIZE;
    }
    if ((uint64_t)PT_IDX(addr) != 0 || (uint64_t)PT_IDX(start_addr) != 0) {
        kprintf("Warning: Address is not 2MB aligned, will only be using small pages!\n");
        num_big_pages = 0;
        num_small_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    }
    for (uint64_t i = 0; i < num_big_pages; i++) {
        map_page_to_virt(start_addr + i * TABLE_SIZE, addr + i * TABLE_SIZE, flags, true);
    }
    for (uint64_t i = 0; i < num_small_pages; i++) {
        map_page_to_virt(start_addr + num_big_pages * TABLE_SIZE + i * PAGE_SIZE, addr + num_big_pages * TABLE_SIZE + i * PAGE_SIZE, flags, false);
    }
}

void FillPageDirectoryUser(void* addr, uint64_t size) {
    FillPageDirectoryGeneral(addr, (void*)((uint64_t)addr - USER_VIRTUAL) ,size, RW_USER);
}

void FillPageDirectoryMMIO(void* addr, uint64_t size) {
    FillPageDirectoryGeneral((void*)((uint64_t)addr + MMIO_OFFSET), addr, size, RW_MMIO);
}

void FillPageDirectoryPCI(void* addr, uint64_t size) {
    FillPageDirectoryGeneral((void*)((uint64_t)addr + PCI_OFFSET), addr, size, RW_MMIO);
}

void FillPageDirectoryIdentityMapping(void* addr, uint64_t size) {
    FillPageDirectoryGeneral(addr, addr, size, RW_KERNEL);
}





void RemoveTables(uint64_t pml4_idx, uint64_t pdpt_idx, uint64_t start_table, uint64_t end_table) {
    page_entry_t* pdpt_entry = (page_entry_t*) ((uint64_t)(pml4[pml4_idx].address) << 12 + KERNEL_VIRTUAL);
    page_entry_t* pd_entry = (page_entry_t*) ((uint64_t)(pdpt_entry[pdpt_idx].address) << 12 + KERNEL_VIRTUAL);
    for (uint64_t i = start_table; i < end_table; i++) {
        if (pd_entry[i].present == 0) {
            continue;
        }
        page_entry_t* pt_entry = (page_entry_t*) ((uint64_t)(pd_entry[i].address) << 12 + KERNEL_VIRTUAL);
        for (uint64_t j = 0; j < 512; j++) {
            if (pt_entry[j].present == 1) {
                pt_entry[j].present = 0;
                InvlpgHelper(((i * 512 + j) * PAGE_SIZE) + (pml4_idx << 39) + (pdpt_idx << 30));
            }
        }
        pd_entry[i].present = 0;
        InvlpgHelper((i * TABLE_SIZE) + (pml4_idx << 39) + (pdpt_idx << 30));
    }
}

void RemovePDs(uint64_t pml4_idx, uint64_t pdpt_idx, uint64_t start_pd, uint64_t end_pd) {
    page_entry_t* pdpt_entry = (page_entry_t*) ((uint64_t)(pml4[pml4_idx].address) << 12 + KERNEL_VIRTUAL);
    for (uint64_t i = start_pd; i < end_pd; i++) {
        if (pdpt_entry[i].present == 1) {
            pdpt_entry[i].present = 0;
            InvlpgHelper((i * TABLE_SIZE) + (pml4_idx << 39) + (pdpt_idx << 30));
        }
    }
}

void RemovePages(uint64_t addr, uint64_t num_pages, bool big_pages) {
    uint64_t addition_size = big_pages ? TABLE_SIZE : PAGE_SIZE;
    for (uint64_t i = 0; i < num_pages; i++) {
        RemovePage(addr + i * addition_size, big_pages);
    }
}

uint64_t AddKernelPages(uint64_t num_pages) {


    uint64_t start_addr = curr_addr;

    for (uint64_t i = 0; i < num_pages; i++) {
        map_page_to_virt(curr_addr, KERNEL_VIRT_TO_PHYS(curr_addr), RW_KERNEL, false);
        curr_addr += PAGE_SIZE;
    }
    return start_addr;
}

uint64_t AddStack() {
    return AddKernelPages(STACK_PAGES);
}

void RemovePage(uint64_t addr, bool big_page) {
    uint64_t pml4_idx = PML4_IDX(addr);
    uint64_t pdpt_idx = PDPT_IDX(addr);
    uint64_t pd_idx = PD_IDX(addr);
    uint64_t t_idx = PT_IDX(addr);

    if (pml4[pml4_idx].present == 0) {
        return;
    }

    page_entry_t* pdpt_entry = (page_entry_t*) ((uint64_t)(pml4[pml4_idx].address) << 12 + KERNEL_VIRTUAL);
    if (pdpt_entry[pdpt_idx].present == 0) {
        return;
    }

    page_entry_t* pd_entry = (page_entry_t*) ((uint64_t)(pdpt_entry[pdpt_idx].address) << 12 + KERNEL_VIRTUAL);
    if (pd_entry[pd_idx].present == 0) {
        return;
    }
    if (big_page) {
        pd_entry[pd_idx].present = 0;
        InvlpgHelper(addr);
        return;
    }
    page_entry_t* pt_entry = (page_entry_t*) ((uint64_t)(pd_entry[pd_idx].address) << 12 + KERNEL_VIRTUAL);
    if (pt_entry[t_idx].present == 0) {
        return;
    }
    pt_entry[t_idx].present = 0;
    InvlpgHelper(addr);
}