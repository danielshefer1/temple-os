#pragma once

#include "includes.h"
#include "elf64_types.h"

// Load an ELF64 binary from `path` into a fresh user address space.
// On success returns 0 and fills *out. On error returns -errno.
//
// `argv` and `envp` may be NULL or NULL-terminated arrays of C strings;
// when non-NULL their contents are laid out on the new user stack per
// the SysV x86-64 initial-stack convention. Strings must already live
// in kernel-side memory (the caller is responsible for copy-from-user).
int64_t load_elf64(const char* path, elf64_image_t* out);
int64_t load_elf64_argv(const char* path,
                        char* const* argv, char* const* envp,
                        elf64_image_t* out);
