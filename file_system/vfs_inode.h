#pragma once

#include "types.h"

int64_t vfs_lookup    (inode_t* dir, dentry_t* d);
int64_t vfs_create    (inode_t* dir, dentry_t* d, uint64_t perm);
int64_t vfs_mkdir     (inode_t* dir, dentry_t* d, uint64_t perm);
int64_t vfs_rmdir     (inode_t* dir, dentry_t* d);
int64_t vfs_unlink    (inode_t* dir, dentry_t* d);
int64_t vfs_rename    (inode_t* old_dir, dentry_t* old_d,
                       inode_t* new_dir, dentry_t* new_d);
int64_t vfs_hardlink  (inode_t* dir, inode_t* existing, dentry_t* d);
int64_t vfs_symlink   (inode_t* dir, dentry_t* d, const char* target);
int64_t vfs_readlink  (inode_t* in, char* buf, uint64_t size);
int64_t vfs_getattr   (inode_t* in, fs_stat_t* out);
int64_t vfs_setattr   (inode_t* in, fs_stat_t* in_stat);
