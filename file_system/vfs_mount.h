#pragma once

#include "types.h"

extern dentry_t* vfs_root;

int64_t  vfs_mount_root   (superblock_t* sb);
int64_t  vfs_unmount_root (void);
dentry_t* vfs_traverse_mount(dentry_t* d);

// Mount `sb` over `target` (a directory dentry on the already-mounted tree).
// Allocates a fresh root dentry for `sb` and stashes it as target->mount_dentry,
// so vfs_traverse_mount jumps from `target` into the new fs. The new root's
// parent pointer is set to target->parent so a ".." walk from inside the
// mounted fs lands at the mount point's parent in the underlying tree.
int64_t vfs_mount_at  (dentry_t* target, superblock_t* sb);
int64_t vfs_unmount_at(dentry_t* target);
