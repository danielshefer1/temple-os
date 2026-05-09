#pragma once
#include "includes.h"
#include "vfs_types.h"
#include "devfs_defs.h"
#include "devfs_types.h"

void devfs_init(void);

// Char and block devices live in the same table but in distinct namespaces:
// (major, minor) is unique within each kind, so block (1, 0) and char (1, 3)
// can coexist (they map to /dev/ram0 and /dev/null in Linux convention).
int64_t devfs_register_char (uint32_t major, uint32_t minor, file_ops_t* fops, void* token);
int64_t devfs_register_block(uint32_t major, uint32_t minor, file_ops_t* fops, void* token);

// Looks up a registered driver. `is_block` selects the namespace.
// Returns NULL if no driver matches.
devfs_entry_t* devfs_lookup(bool is_block, uint32_t major, uint32_t minor);
