#pragma once

#include "types.h"

// constructors / destructors — the *only* dentry/inode allocation entry points
dentry_t* vfs_dentry_alloc (dentry_t* parent, const char* name);
void      vfs_dentry_free  (dentry_t* d);

// dcache-aware lookup: returns existing or freshly populated dentry.
// On miss returns NULL (lookup error or OOM); errno-style detail not preserved.
dentry_t* vfs_dentry_get   (dentry_t* parent, const char* name);

// inode load/release — wrap alloc_inode + read_inode and free_inode
inode_t*  vfs_iget         (superblock_t* sb);
void      vfs_iput         (inode_t* in);
