#pragma once
#include "includes.h"
#include "vfs_types.h"

// In-memory /proc filesystem. Modeled on Linux: dynamic inode generation,
// lazy lookup of /proc/<pid>, content generated on read. Mounted via
// vfs_mount_at over an ext2 directory.
//
// Initialization: call procfs_init() once before mounting. Then build a
// superblock with procfs_create_sb() and pass it to vfs_mount_at(target, sb).

void          procfs_init     (void);
superblock_t* procfs_create_sb(void);
