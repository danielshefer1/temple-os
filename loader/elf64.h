#pragma once

#include "includes.h"
#include "elf64_types.h"

// Load an ELF64 binary from `path` into a fresh user address space.
// On success returns 0 and fills *out. On error returns -errno.
int64_t load_elf64(const char* path, elf64_image_t* out);
