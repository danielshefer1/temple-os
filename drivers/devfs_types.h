#pragma once
#include "includes.h"
#include "vfs_types.h"

typedef struct devfs_entry_t {
    uint32_t      major;
    uint32_t      minor;
    bool          is_block;   // true ⇒ block device, false ⇒ char device
    file_ops_t*   fops;       // NULL slot ⇒ unused
    void*         token;      // copied into file_t.private_data on open
} devfs_entry_t;
