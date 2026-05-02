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
    // kmalloc(PAGE_SIZE) returns slab-backed 4KB-aligned page reachable at
    // KERNEL_VIRTUAL+phys. Used here instead of AddKernelPages because the
    // buddy allocator's GetBuddyAddress has an off-by-one and can return
    // 2KB-aligned pages for 4KB requests, which fails as a CR3 value.
    void* p = kmalloc(PAGE_SIZE);
    if (!p) return -ENOMEM;
    if ((uint64_t)p & 0xFFF) return -EINVAL;
    memset(p, 0, PAGE_SIZE);

    page_entry_t* new_pml4 = (page_entry_t*)p;
    // Share the kernel half (entries 256..511). Entry 0 (identity) stays
    // empty — the boot identity map was disabled by DisableIdentityMapping.
    for (uint64_t i = 256; i < 512; i++) new_pml4[i] = pml4[i];

    *out_phys = KERNEL_VIRT_TO_PHYS((uint64_t)p);
    return 0;
}

void free_cloned_pml4(uint64_t pml4_phys) {
    kfree((void*)(pml4_phys + KERNEL_VIRTUAL), PAGE_SIZE);
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
