#pragma once

#include "types.h"

extern dentry_t* vfs_root;

int64_t  vfs_mount_root   (superblock_t* sb);
int64_t  vfs_unmount_root (void);
dentry_t* vfs_traverse_mount(dentry_t* d);
