#include "paging.h"
#include "global.h"
#include "cpu_local.h"
#include "apic.h"

#define TLB_SHOOTDOWN_VECTOR 65

static tlb_shootdown_t shootdown;

static inline uint64_t online_cpu_mask_excluding_self(uint32_t self_idx) {
    uint64_t online = cpus_active;
    if (online > 64) online = 64;
    uint64_t mask = (online >= 64) ? ~0ULL : ((1ULL << online) - 1ULL);
    mask &= ~(1ULL << self_idx);
    return mask;
}

void tlb_flush_remote(uint64_t addr) {
    // Local invalidation first — covers the (common) single-CPU case and
    // the initiator side of the broadcast.
    if (addr == 0) flush_tlb();
    else           InvlpgHelper(addr);

    // Snapshot online CPU count. APs that come up after this call are
    // booting on the same PML4 we're modifying, so they don't have stale
    // TLB entries for `addr` to begin with.
    uint64_t online = cpus_active;
    if (online <= 1) return;

    cpu_local_t* self = this_cpu();
    uint64_t target = online_cpu_mask_excluding_self(self->cpu_index);
    if (target == 0) return;

    spin_lock(&shootdown.lock);
    shootdown.addr = addr;
    __atomic_store_n(&shootdown.pending, target, __ATOMIC_RELEASE);
    SendIpiAllExcludingSelf(TLB_SHOOTDOWN_VECTOR);
    while (__atomic_load_n(&shootdown.pending, __ATOMIC_ACQUIRE) != 0) {
        PauseHelper();
    }
    spin_unlock(&shootdown.lock);
}

void TlbShootdownHandler(void) {
    uint64_t addr = shootdown.addr;
    if (addr == 0) flush_tlb();
    else           InvlpgHelper(addr);

    cpu_local_t* self = this_cpu();
    __atomic_and_fetch(&shootdown.pending,
                       ~(1ULL << self->cpu_index),
                       __ATOMIC_RELEASE);
}

page_entry_t pml4[512] __attribute__((aligned(4096)));

page_entry_t kernel_pdpt[512] __attribute__((aligned(4096)));
page_entry_t kernel_pd[512] __attribute__((aligned(4096)));
page_entry_t kernel_pt[512] __attribute__((aligned(4096)));

page_entry_t identity_pdpt[512] __attribute__((aligned(4096)));
page_entry_t identity_pd[512] __attribute__((aligned(4096)));

static uint64_t curr_addr_prim;

// Lock ordering for memory subsystem: paging > buddy > slab. The lock
// covers kernel page-table mutation. map_page_to_virt and RemovePage hold
// it; AddKernelPages does NOT (RequestBuddy/map_page_to_virt take their
// own locks). Same-CPU recursion via map_page_to_virt -> kmalloc(PAGE_SIZE)
// -> AddSlabW -> AddKernelPages -> map_page_to_virt would deadlock; relies
// on the PAGE_SIZE slab cache staying warm.
static spinlock_t paging_lock;

void InitPaging() {
    uint64_t kernel_size = (uint64_t)&__kernel_size_bytes;
    uint64_t text_size = (uint64_t)&__text_size;
    uint64_t stack_bottom = (uint64_t)&_stack_bottom; 

    uint64_t kernel_big_pages = (kernel_size + TABLE_SIZE - 1) / TABLE_SIZE;
    uint64_t text_big_pages = (text_size + TABLE_SIZE - 1) / TABLE_SIZE;

    curr_addr_prim = KERNEL_VIRTUAL + (((kernel_size + PAGE_SIZE - 1) / PAGE_SIZE) + 8)*PAGE_SIZE;

    uint64_t kernel_pml4_idx = PML4_IDX(KERNEL_VIRTUAL);
    uint64_t kernel_pdpt_idx = PDPT_IDX(KERNEL_VIRTUAL);
    uint64_t base_idx = PD_IDX(KERNEL_VIRTUAL);


    // Start PML4 Mapping
    pml4[kernel_pml4_idx].present = 1;
    pml4[kernel_pml4_idx].writable = 1;
    pml4[kernel_pml4_idx].address = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)kernel_pdpt) >> 12;

    pml4[0].present = 1;
    pml4[0].writable = 1;
    pml4[0].address = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)identity_pdpt) >> 12;
    // End PML4 Mapping

    // Start PDPT Mapping
    kernel_pdpt[kernel_pdpt_idx].present = 1;
    kernel_pdpt[kernel_pdpt_idx].writable = 1;
    kernel_pdpt[kernel_pdpt_idx].address = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)kernel_pd) >> 12;

    identity_pdpt[0].present = 1;
    identity_pdpt[0].writable = 1;
    identity_pdpt[0].address = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)identity_pd) >> 12;
    // Start PDPT Mapping

    identity_pd[0].present = 1;
    identity_pd[0].writable = 1;
    identity_pd[0].page_size = 1;
    identity_pd[0].address = 0;

    // Start PD Mapping
    for (uint64_t i = 0; i < kernel_big_pages; i++) {
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

    // Last big page mapped with 4KB pages to not fragment and add a guard page before stack
    uint64_t remaining_kernel_size = kernel_size - ((kernel_big_pages - 1) * TABLE_SIZE);
    uint64_t num_small_pages = (remaining_kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (num_small_pages == 0) {
        switch_pml4((page_entry_t*)KERNEL_VIRT_TO_PHYS((uint64_t)pml4));
        return;
    }
    page_entry_t* last_pd_entry = &kernel_pd[base_idx + kernel_big_pages];
    last_pd_entry->present = 1;
    last_pd_entry->writable = 1;
    last_pd_entry->address = KERNEL_VIRT_TO_PHYS((uint64_t)kernel_pt) >> 12;
    last_pd_entry->global = 1;
    last_pd_entry->no_execute = (text_big_pages == kernel_big_pages) ? 0 : 1;

    for (uint64_t i = 0; i < num_small_pages; i++) {
        kernel_pt[i].present = 1;
        kernel_pt[i].writable = ((text_big_pages == kernel_big_pages) && (i*PAGE_SIZE < text_size % TABLE_SIZE)) ? 1 : 0;
        kernel_pt[i].address = (2*MB * (kernel_big_pages) + i*PAGE_SIZE) >> 12;
        kernel_pt[i].global = 1;
        kernel_pt[i].no_execute = ((text_big_pages == kernel_big_pages) && (i*PAGE_SIZE < text_size % TABLE_SIZE)) ? 0 : 1;

        if ((2*MB * (kernel_big_pages) + i*PAGE_SIZE) < stack_bottom && (2*MB * (kernel_big_pages) + (i+1)*PAGE_SIZE) >= stack_bottom) {
            kernel_pt[i].present = 0; // Set guard page before the stack, NEED TO MAKE SURE THERE'S PADDING IN THE BSS
        }
    }


    // End PD Mapping    
    switch_pml4((page_entry_t*)KERNEL_VIRT_TO_PHYS((uint64_t)pml4));
}

uint64_t GetCurrPrimitveAddr() { return curr_addr_prim;}

uint64_t PageDirAddrV() {
    return (uint64_t)pml4;
}

void DisableIdentityMapping() {
    pml4[0].present = 0;
    switch_pml4((page_entry_t*)KERNEL_VIRT_TO_PHYS(pml4));
    // APs share this PML4 — they need to drop their cached identity-map
    // entries too. Full flush via the shootdown (addr=0 means CR3 reload).
    tlb_flush_remote(0);
}

void map_page_to_virt(uint64_t virt, uint64_t phy, uint64_t flags, bool big_page) {
    uint64_t pml4_idx = PML4_IDX(virt);
    uint64_t pdpt_idx = PDPT_IDX(virt);
    uint64_t pd_idx = PD_IDX(virt);
    uint64_t pt_idx = PT_IDX(virt);

    bool is_user = (flags & USER_PAGE) ? true : false;
    page_entry_t* new_pdpt, *new_pd;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&paging_lock);

    if (pml4[pml4_idx].present == 0) {
        new_pdpt = (page_entry_t*) kmalloc(PAGE_SIZE);
        memset(new_pdpt, 0, PAGE_SIZE);
        pml4[pml4_idx].present = 1;
        pml4[pml4_idx].writable = 1;
        pml4[pml4_idx].address = KERNEL_VIRT_TO_PHYS((uint64_t)new_pdpt) >> 12;
    }
    if (is_user) pml4[pml4_idx].user = 1;
    page_entry_t* pdpt = (page_entry_t*) ((pml4[pml4_idx].address << 12) + KERNEL_VIRTUAL);
    if (pdpt[pdpt_idx].present == 0) {
        new_pd = (page_entry_t*) kmalloc(PAGE_SIZE);
        memset(new_pd, 0, PAGE_SIZE);
        pdpt[pdpt_idx].present = 1;
        pdpt[pdpt_idx].writable = 1;
        pdpt[pdpt_idx].address = KERNEL_VIRT_TO_PHYS((uint64_t)new_pd) >> 12;
    }
    if (is_user) pdpt[pdpt_idx].user = 1;

    page_entry_t* pd = (page_entry_t*) ((pdpt[pdpt_idx].address << 12) + KERNEL_VIRTUAL);

    if (big_page) {
        if (new_pd == NULL) {
            spin_unlock(&paging_lock);
            if (ie) StiHelper();
            kprintf("Tried to map a big page but page directory already exists!");
            return;
        }
        if (pt_idx != 0 && PT_IDX(phy) != 0) {
            //kprintf("Tried to map a big page but not 2MB aligned!");
            spin_unlock(&paging_lock);
            if (ie) StiHelper();
            return;
        }
        pd[pd_idx].present = 1;
        pd[pd_idx].writable = (flags & RW_PAGE) ? 1 : 0;
        pd[pd_idx].page_size = 1;
        pd[pd_idx].user = (flags & USER_PAGE) ? 1 : 0;
        pd[pd_idx].address = phy >> 12;
        pd[pd_idx].no_execute = (flags & NX_PAGE) ? 1 : 0;
        pd[pd_idx].pcd = (flags & CACHE_DIS_PAGE) ? 1 : 0;
        pd[pd_idx].pwt = (flags & WRITE_THROUGH_PAGE) ? 1 : 0;
        pd[pd_idx].global = (flags & GLOBAL_PAGE) ? 1 : 0;
        pd[pd_idx].page_size = 1;
        spin_unlock(&paging_lock);
        if (ie) StiHelper();
        return;
    }


    if (pd[pd_idx].present == 0) {
        page_entry_t* new_pt = (page_entry_t*) kmalloc(PAGE_SIZE);
        memset(new_pt, 0, PAGE_SIZE);
        pd[pd_idx].present = 1;
        pd[pd_idx].writable = 1;
        pd[pd_idx].address = KERNEL_VIRT_TO_PHYS((uint64_t)new_pt) >> 12;
    }
    if (is_user) pd[pd_idx].user = 1;

    if (pd[pd_idx].page_size == 1) {
        //kprintf("Warning: ID 50\t");
        spin_unlock(&paging_lock);
        if (ie) StiHelper();
        return;
    }

    page_entry_t* pt = (page_entry_t*) ((pd[pd_idx].address << 12) + KERNEL_VIRTUAL);

    pt[pt_idx].present = 1;
    pt[pt_idx].writable = (flags & RW_PAGE) ? 1 : 0;
    pt[pt_idx].user = (flags & USER_PAGE) ? 1 : 0;
    pt[pt_idx].address = phy >> 12;
    pt[pt_idx].no_execute = (flags & NX_PAGE) ? 1 : 0;
    pt[pt_idx].pcd = (flags & CACHE_DIS_PAGE) ? 1 : 0;
    pt[pt_idx].pwt = (flags & WRITE_THROUGH_PAGE) ? 1 : 0;
    pt[pt_idx].global = (flags & GLOBAL_PAGE) ? 1 : 0;
    spin_unlock(&paging_lock);
    if (ie) StiHelper();
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
        //kprintf("Warning: Address is not 2MB aligned, will only be using small pages!\n");
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
    page_entry_t* pdpt_entry = (page_entry_t*) (((uint64_t)(pml4[pml4_idx].address) << 12) + KERNEL_VIRTUAL);
    page_entry_t* pd_entry = (page_entry_t*) (((uint64_t)(pdpt_entry[pdpt_idx].address) << 12) + KERNEL_VIRTUAL);
    for (uint64_t i = start_table; i < end_table; i++) {
        if (pd_entry[i].present == 0) {
            continue;
        }
        page_entry_t* pt_entry = (page_entry_t*) (((uint64_t)(pd_entry[i].address) << 12) + KERNEL_VIRTUAL);
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
    page_entry_t* pdpt_entry = (page_entry_t*) (((uint64_t)(pml4[pml4_idx].address) << 12) + KERNEL_VIRTUAL);
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

uint64_t AddKernelPagesPrimitive(uint64_t num_pages) {
    uint64_t start_addr = curr_addr_prim;

    for (uint64_t i = 0; i < num_pages; i++) {
        map_page_to_virt(curr_addr_prim, KERNEL_VIRT_TO_PHYS(curr_addr_prim), RW_KERNEL, false);
        curr_addr_prim += PAGE_SIZE;
    }
    return start_addr;
}

uint64_t AddKernelPages(uint64_t num_pages) {
    uint64_t start_addr = ((uint64_t)RequestBuddy(num_pages * PAGE_SIZE, false)) + KERNEL_VIRTUAL, curr_addr = start_addr;
    uint64_t pages_in_big = 2*MB/PAGE_SIZE;

    while (num_pages >= 2*MB/PAGE_SIZE) {
        map_page_to_virt(curr_addr, KERNEL_VIRT_TO_PHYS(curr_addr), RW_KERNEL, true);
        num_pages -= 2*MB/PAGE_SIZE;
        curr_addr += 2*MB;
    }
    while (num_pages > 0) {
        map_page_to_virt(curr_addr, KERNEL_VIRT_TO_PHYS(curr_addr), RW_KERNEL, true);
        num_pages--;
        curr_addr += PAGE_SIZE;
    }
    return start_addr;
}

uint64_t AddNonCachableKernelPages(uint64_t num_pages) {
    uint64_t start_addr = ((uint64_t)RequestBuddy(num_pages * PAGE_SIZE, false)) + KERNEL_VIRTUAL, curr_addr = start_addr;
    if (start_addr == KERNEL_VIRTUAL) return 0;
    uint64_t pages_in_big = 2*MB/PAGE_SIZE;

    while (num_pages >= pages_in_big) {
        map_page_to_virt(curr_addr, KERNEL_VIRT_TO_PHYS(curr_addr), RW_MMIO, true);
        num_pages -= pages_in_big;
        curr_addr += 2*MB;
    }
    while (num_pages > 0) {
        map_page_to_virt(curr_addr, KERNEL_VIRT_TO_PHYS(curr_addr), RW_MMIO, true);
        num_pages--;
        curr_addr += PAGE_SIZE;
    }
    return start_addr;
}

void RemoveKernelPages(uint64_t start, uint64_t num_pages) {
    uint64_t curr_addr = start, pages_in_big = 2*MB/PAGE_SIZE;

    FreeBuddy((void*) KERNEL_VIRT_TO_PHYS(start), false);
    while (num_pages >= pages_in_big) {
        RemovePage(curr_addr, true);
        num_pages -= pages_in_big;
        curr_addr += 2*MB;
    }
    while (num_pages > 0) {
        RemovePage(curr_addr, false);
        num_pages--;
        curr_addr += PAGE_SIZE;
    }
}

uint64_t AddStack() {
    return AddKernelPages(STACK_PAGES);
}

void RemovePage(uint64_t addr, bool big_page) {
    uint64_t pml4_idx = PML4_IDX(addr);
    uint64_t pdpt_idx = PDPT_IDX(addr);
    uint64_t pd_idx = PD_IDX(addr);
    uint64_t t_idx = PT_IDX(addr);

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&paging_lock);
    bool need_shootdown = false;

    if (pml4[pml4_idx].present == 0) {
        goto out;
    }

    page_entry_t* pdpt_entry = (page_entry_t*) (((uint64_t)(pml4[pml4_idx].address) << 12) + KERNEL_VIRTUAL);
    if (pdpt_entry[pdpt_idx].present == 0) {
        goto out;
    }

    page_entry_t* pd_entry = (page_entry_t*) (((uint64_t)(pdpt_entry[pdpt_idx].address) << 12) + KERNEL_VIRTUAL);
    if (pd_entry[pd_idx].present == 0) {
        goto out;
    }
    if (big_page) {
        pd_entry[pd_idx].present = 0;
        need_shootdown = true;
        goto out;
    }
    page_entry_t* pt_entry = (page_entry_t*) (((uint64_t)(pd_entry[pd_idx].address) << 12) + KERNEL_VIRTUAL);
    if (pt_entry[t_idx].present == 0) {
        goto out;
    }
    pt_entry[t_idx].present = 0;
    need_shootdown = true;

out:
    spin_unlock(&paging_lock);
    // Run the shootdown after releasing paging_lock: another CPU may be
    // spinning on paging_lock with interrupts disabled, and could not
    // service our shootdown IPI until it makes forward progress. Doing the
    // local+remote invalidation here keeps the lock-hold window short.
    if (need_shootdown) tlb_flush_remote(addr);
    if (ie) StiHelper();
}