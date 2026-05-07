#pragma once
#include "includes.h"
#include "vfs_types.h"

// Linux old-encoding: dev = (major << 8) | minor (8-bit fields, max 256).
// Stored on disk in ext2_inode_disk_t.i_block[0]. Sufficient for our needs;
// upgrade to the 32-bit "new" encoding if/when we need majors > 255.
#define MKDEV(maj, min)  ((((uint32_t)(maj) & 0xFFu) << 8) | ((uint32_t)(min) & 0xFFu))
#define MAJOR(dev)       (((uint32_t)(dev) >> 8) & 0xFFu)
#define MINOR(dev)       ((uint32_t)(dev) & 0xFFu)

#define DEVFS_MAX_DEVICES 32

typedef struct devfs_entry_t {
    uint32_t      major;
    uint32_t      minor;
    bool          is_block;   // true ⇒ block device, false ⇒ char device
    file_ops_t*   fops;       // NULL slot ⇒ unused
    void*         token;      // copied into file_t.private_data on open
} devfs_entry_t;

void devfs_init(void);

// Char and block devices live in the same table but in distinct namespaces:
// (major, minor) is unique within each kind, so block (1, 0) and char (1, 3)
// can coexist (they map to /dev/ram0 and /dev/null in Linux convention).
int64_t devfs_register_char (uint32_t major, uint32_t minor, file_ops_t* fops, void* token);
int64_t devfs_register_block(uint32_t major, uint32_t minor, file_ops_t* fops, void* token);

// Looks up a registered driver. `is_block` selects the namespace.
// Returns NULL if no driver matches.
devfs_entry_t* devfs_lookup(bool is_block, uint32_t major, uint32_t minor);
