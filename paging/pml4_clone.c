#include "pml4_clone.h"
#include "paging.h"
#include "memory.h"
#include "string.h"
#include "extern.h"
#include "defintions.h"
#include "buddy_alloc.h"
#include "slab_alloc.h"

extern page_entry_t pml4[512];

static inline page_entry_t* table_kvirt(uint64_t phys) {
    return (page_entry_t*)(phys + KERNEL_VIRTUAL);
}

int64_t clone_kernel_pml4(uint64_t* out_phys) {
    if (!out_phys) return -EINVAL;
    void* phys = RequestBuddy(PAGE_SIZE, false);
    if (!phys) return -ENOMEM;
    page_entry_t* new_pml4 = (page_entry_t*)((uint64_t)phys + KERNEL_VIRTUAL);
    memset(new_pml4, 0, PAGE_SIZE);

    // Share the kernel half (entries 256..511). Entry 0 (identity) stays
    // empty — the boot identity map was disabled by DisableIdentityMapping.
    for (uint64_t i = 256; i < 512; i++) new_pml4[i] = pml4[i];

    *out_phys = (uint64_t)phys;
    return 0;
}

void free_cloned_pml4(uint64_t pml4_phys) {
    FreeBuddy((void*)pml4_phys, false);
}

int64_t lookup_user_in_pml4(uint64_t pml4_phys, uint64_t virt, uint64_t* out_phys) {
    page_entry_t* p4 = table_kvirt(pml4_phys);
    uint64_t i4 = PML4_IDX(virt), i3 = PDPT_IDX(virt), i2 = PD_IDX(virt), i1 = PT_IDX(virt);
    if (!p4[i4].present) return -ENOENT;
    page_entry_t* p3 = table_kvirt((uint64_t)p4[i4].address << 12);
    if (!p3[i3].present) return -ENOENT;
    page_entry_t* p2 = table_kvirt((uint64_t)p3[i3].address << 12);
    if (!p2[i2].present) return -ENOENT;
    page_entry_t* p1 = table_kvirt((uint64_t)p2[i2].address << 12);
    if (!p1[i1].present) return -ENOENT;
    *out_phys = (uint64_t)p1[i1].address << 12;
    return 0;
}
