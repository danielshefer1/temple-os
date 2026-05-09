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

int64_t clone_user_pml4(uint64_t parent_pml4_phys, uint64_t* out_child_phys) {
    if (!out_child_phys) return -EINVAL;

    uint64_t child_phys = 0;
    int64_t r = clone_kernel_pml4(&child_phys);
    if (r < 0) return r;

    page_entry_t* p4 = table_kvirt(parent_pml4_phys);
    page_entry_t* c4 = table_kvirt(child_phys);

    for (uint64_t i4 = 0; i4 < 256; i4++) {
        if (!p4[i4].present) continue;

        void* c_pdpt = RequestBuddy(PAGE_SIZE, false);
        if (!c_pdpt) { r = -ENOMEM; goto fail; }
        page_entry_t* p3 = table_kvirt((uint64_t)p4[i4].address << 12);
        page_entry_t* c3 = table_kvirt((uint64_t)c_pdpt);
        memset(c3, 0, PAGE_SIZE);
        c4[i4] = p4[i4];
        c4[i4].address = (uint64_t)c_pdpt >> 12;

        for (uint64_t i3 = 0; i3 < 512; i3++) {
            if (!p3[i3].present) continue;
            if (p3[i3].page_size) { r = -ENOTSUP; goto fail; }

            void* c_pd = RequestBuddy(PAGE_SIZE, false);
            if (!c_pd) { r = -ENOMEM; goto fail; }
            page_entry_t* p2 = table_kvirt((uint64_t)p3[i3].address << 12);
            page_entry_t* c2 = table_kvirt((uint64_t)c_pd);
            memset(c2, 0, PAGE_SIZE);
            c3[i3] = p3[i3];
            c3[i3].address = (uint64_t)c_pd >> 12;

            for (uint64_t i2 = 0; i2 < 512; i2++) {
                if (!p2[i2].present) continue;
                if (p2[i2].page_size) { r = -ENOTSUP; goto fail; }

                void* c_pt = RequestBuddy(PAGE_SIZE, false);
                if (!c_pt) { r = -ENOMEM; goto fail; }
                page_entry_t* p1 = table_kvirt((uint64_t)p2[i2].address << 12);
                page_entry_t* c1 = table_kvirt((uint64_t)c_pt);
                memset(c1, 0, PAGE_SIZE);
                c2[i2] = p2[i2];
                c2[i2].address = (uint64_t)c_pt >> 12;

                for (uint64_t i1 = 0; i1 < 512; i1++) {
                    if (!p1[i1].present) continue;
                    // Device/MMIO mappings (e.g. /dev/fb mapped via
                    // MmapFileHandler with PCD set) point at hardware
                    // physical pages — those aren't owned by the buddy and
                    // (phys + KERNEL_VIRTUAL) may not even be a valid
                    // kernel-virt alias since the kernel mapping doesn't
                    // cover the full phys range. Alias the PTE so child and
                    // parent share the same device pages; do NOT memcpy.
                    if (p1[i1].pcd) {
                        c1[i1] = p1[i1];
                        continue;
                    }
                    void* new_page = RequestBuddy(PAGE_SIZE, false);
                    if (!new_page) { r = -ENOMEM; goto fail; }
                    void* parent_kvirt = (void*)(((uint64_t)p1[i1].address << 12) + KERNEL_VIRTUAL);
                    void* child_kvirt  = (void*)((uint64_t)new_page + KERNEL_VIRTUAL);
                    memcpy(child_kvirt, parent_kvirt, PAGE_SIZE);
                    c1[i1] = p1[i1];
                    c1[i1].address = (uint64_t)new_page >> 12;
                }
            }
        }
    }

    *out_child_phys = child_phys;
    return 0;

fail:
    free_user_address_space(child_phys);
    free_cloned_pml4(child_phys);
    return r;
}

void free_user_address_space(uint64_t pml4_phys) {
    page_entry_t* p4 = table_kvirt(pml4_phys);
    for (uint64_t i4 = 0; i4 < 256; i4++) {
        if (!p4[i4].present) continue;
        uint64_t pdpt_phys = (uint64_t)p4[i4].address << 12;
        page_entry_t* p3 = table_kvirt(pdpt_phys);
        for (uint64_t i3 = 0; i3 < 512; i3++) {
            if (!p3[i3].present) continue;
            // 1 GiB page at PDPT level: free the backing region and move on.
            if (p3[i3].page_size) {
                FreeBuddy((void*)((uint64_t)p3[i3].address << 12), false);
                memset(&p3[i3], 0, sizeof(p3[i3]));
                continue;
            }
            uint64_t pd_phys = (uint64_t)p3[i3].address << 12;
            page_entry_t* p2 = table_kvirt(pd_phys);
            for (uint64_t i2 = 0; i2 < 512; i2++) {
                if (!p2[i2].present) continue;
                if (p2[i2].page_size) {
                    // 2 MiB page: free the backing region.
                    FreeBuddy((void*)((uint64_t)p2[i2].address << 12), false);
                    memset(&p2[i2], 0, sizeof(p2[i2]));
                    continue;
                }
                uint64_t pt_phys = (uint64_t)p2[i2].address << 12;
                page_entry_t* p1 = table_kvirt(pt_phys);
                for (uint64_t i1 = 0; i1 < 512; i1++) {
                    if (!p1[i1].present) continue;
                    // Aliased device pages (PCD set, see clone_user_pml4)
                    // are owned by the device, not the buddy. Just clear
                    // the PTE — never feed MMIO phys back to FreeBuddy.
                    if (p1[i1].pcd) {
                        memset(&p1[i1], 0, sizeof(p1[i1]));
                        continue;
                    }
                    FreeBuddy((void*)((uint64_t)p1[i1].address << 12), false);
                    memset(&p1[i1], 0, sizeof(p1[i1]));
                }
                FreeBuddy((void*)pt_phys, false);
                memset(&p2[i2], 0, sizeof(p2[i2]));
            }
            FreeBuddy((void*)pd_phys, false);
            memset(&p3[i3], 0, sizeof(p3[i3]));
        }
        FreeBuddy((void*)pdpt_phys, false);
        memset(&p4[i4], 0, sizeof(p4[i4]));
    }
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
