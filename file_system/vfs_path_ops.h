#pragma once

#include "types.h"

int64_t vfs_open_path     (const char* path, uint32_t flags, uint64_t mode,
                           file_t** out);
int64_t vfs_create_path   (const char* path, uint64_t perm);
int64_t vfs_unlink_path   (const char* path);
int64_t vfs_mkdir_path    (const char* path, uint64_t perm);
int64_t vfs_rmdir_path    (const char* path);
int64_t vfs_rename_path   (const char* old_path, const char* new_path);
int64_t vfs_symlink_path  (const char* target, const char* linkpath);
int64_t vfs_readlink_path (const char* path, char* buf, uint64_t size);
int64_t vfs_stat_path     (const char* path, fs_inode_stat_t* out);
int64_t vfs_mknod_path    (const char* path, uint64_t type,
                           uint64_t perm, uint32_t dev_id);