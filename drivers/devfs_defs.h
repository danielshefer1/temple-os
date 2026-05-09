#pragma once
#include "includes.h"

// Linux old-encoding: dev = (major << 8) | minor (8-bit fields, max 256).
// Stored on disk in ext2_inode_disk_t.i_block[0]. Sufficient for our needs;
// upgrade to the 32-bit "new" encoding if/when we need majors > 255.
#define MKDEV(maj, min)  ((((uint32_t)(maj) & 0xFFu) << 8) | ((uint32_t)(min) & 0xFFu))
#define MAJOR(dev)       (((uint32_t)(dev) >> 8) & 0xFFu)
#define MINOR(dev)       ((uint32_t)(dev) & 0xFFu)

#define DEVFS_MAX_DEVICES 32
