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

// Look up the physical page backing `virt` in the given PML4. Returns 0 on
// success and stores the phys page (4 KiB aligned, no flags) in *out_phys.
// Returns -ENOENT if not mapped.
int64_t lookup_user_in_pml4(uint64_t pml4_phys, uint64_t virt, uint64_t* out_phys);
