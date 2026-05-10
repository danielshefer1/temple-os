#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "buddy_alloc.h"

uint64_t PageDirAddrV();
void InitPaging();

// Reprogram this CPU's IA32_PAT so PA1 = WC. Must be called once per CPU
// (BSP and each AP) before any framebuffer access; without it, RW_FB pages
// fall back to whatever the reset-default PAT slot 1 type is (WT).
void pat_init(void);
void DisableIdentityMapping();
uint64_t AddKernelPages(uint64_t num_pages);
void RemoveKernelPages(uint64_t start, uint64_t num_pages);
uint64_t AddKernelPagesPrimitive(uint64_t num_pages);
uint64_t AddNonCachableKernelPages(uint64_t num_pages);
void FillPageDirectoryUser(void* addr, uint64_t size);
void FillPageDirectoryMMIO(void* addr, uint64_t size);
void FillPageDirectoryPCI(void* addr, uint64_t size);
void FillPageDirectoryIdentityMapping(void* addr, uint64_t size);
void RemovePages(uint64_t addr, uint64_t num_pages, bool big_pages);
uint64_t AddStack();
void RemovePage(uint64_t addr, bool big_page);
void map_page_to_virt(uint64_t virt, uint64_t phy, uint64_t flags, bool big_page);

// Same as map_page_to_virt, but operates on a caller-supplied PML4. Used by
// the ELF loader to populate per-task user address spaces without touching
// the global kernel pml4. The PML4 base is the kernel-virt of the table page
// (i.e. cr3_phys + KERNEL_VIRTUAL), since every page table this kernel
// allocates is reachable at that alias.
//
// Returns 0 on success, -ENOMEM if a page-table allocation fails, -EEXIST
// when a 4KB mapping collides with an existing big-page entry. The big-page
// no-op cases (PD already populated, or unaligned phys) return 0 — these
// are intentional behavior that AddKernelPages relies on.
int64_t map_page_to_virt_in(page_entry_t* pml4_base,
                            uint64_t virt, uint64_t phy, uint64_t flags, bool big_page);

// Clear the 4KB mapping for `virt` in the given PML4. Returns 0 if it was
// present (and a TLB shootdown was issued), -ENOENT if no mapping existed.
// Page-table pages above the PT level are left in place — free_user_address_space
// reaps them when the task exits.
int64_t unmap_page_in(page_entry_t* pml4_base, uint64_t virt);
uint64_t GetCurrPrimitveAddr();

// Invalidate `addr` (single page) on the local TLB and broadcast a TLB
// shootdown IPI to every other online CPU. Pass 0 for a full-flush
// shootdown (CR3 reload on each CPU). Must NOT be called from interrupt
// context, and must NOT be called while holding any lock that another CPU
// is spinning on with interrupts disabled (see paging.c notes).
void tlb_flush_remote(uint64_t addr);

// Vector-65 IPI handler. Reads the single in-flight shootdown descriptor
// and clears this CPU's pending bit.
void TlbShootdownHandler(void);