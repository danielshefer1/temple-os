#pragma once

#include "includes.h"
#include "types.h"

// Allocate a fresh PML4 page, copy the kernel-half (entries 256..511) of the
// global pml4 into it, and return its physical address through *out_phys.
// Returns 0 on success, -ENOMEM on failure.
int64_t clone_kernel_pml4(uint64_t* out_phys);

// Free a PML4 previously returned by clone_kernel_pml4 (kernel-only mappings;
// callers that populated user-half mappings must tear them down first).
void free_cloned_pml4(uint64_t pml4_phys);

// Walk the user half (PML4 entries 0..255) of `pml4_phys` and free every
// backing page plus the intermediate PT/PD/PDPT pages. Clears the freed
// PML4 entries to zero. Does NOT free the PML4 page itself — call
// free_cloned_pml4 after this if the PML4 is also being discarded.
//
// The PML4 may belong to any task, including one that is not currently
// loaded in CR3; the walk uses kernel-virt aliases (phys + KERNEL_VIRTUAL).
// If it is the active CR3, the caller must arrange a CR3 swap to a
// different PML4 first to avoid pulling pages out from under itself.
void free_user_address_space(uint64_t pml4_phys);

// Look up the physical page backing `virt` in the given PML4. Returns 0 on
// success and stores the phys page (4 KiB aligned, no flags) in *out_phys.
// Returns -ENOENT if not mapped.
int64_t lookup_user_in_pml4(uint64_t pml4_phys, uint64_t virt, uint64_t* out_phys);

// Deep-copy the user half of `parent_pml4_phys` into a fresh PML4 (kernel
// half copied by clone_kernel_pml4). Every present user page is duplicated:
// the child gets its own physical pages with byte-identical contents, so
// post-fork writes by either side are independent. On success returns 0
// and stores the new PML4 phys in *out_child_phys. On OOM tears down any
// partial child state and returns -ENOMEM. 1 GiB / 2 MiB pages in the
// user half are not supported (return -ENOTSUP) — user code only ever
// allocates 4 KiB pages today.
int64_t clone_user_pml4(uint64_t parent_pml4_phys, uint64_t* out_child_phys);
