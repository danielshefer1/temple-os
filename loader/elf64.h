#pragma once

#include "includes.h"

typedef struct elf64_image_t {
    uint64_t entry;        // virtual entry point (already includes load_bias)
    uint64_t base;         // load_bias
    uint64_t brk;          // first byte past last PT_LOAD page
    uint64_t stack_top;    // initial RSP for user (points at argc)
    uint64_t cr3_phys;     // physical address of new PML4
} elf64_image_t;

// Load an ELF64 binary from `path` into a fresh user address space.
// On success returns 0 and fills *out. On error returns -errno.
int64_t load_elf64(const char* path, elf64_image_t* out);
