#pragma once

#include "types.h"

// op-vector wrappers
int64_t vfs_read     (file_t* f, void* buf, uint64_t size);
int64_t vfs_write    (file_t* f, const void* buf, uint64_t size);
int64_t vfs_seek     (file_t* f, int64_t off, int64_t whence);
int64_t vfs_truncate (file_t* f, uint64_t new_size);
int64_t vfs_readdir  (file_t* f, dentry_t* out);
int64_t vfs_open     (inode_t* in, file_t* f);
int64_t vfs_close    (file_t* f);
int64_t vfs_flush    (file_t* f);
int64_t vfs_ioctl    (file_t* f, uint64_t cmd, void* arg);

// file_t lifecycle
file_t* vfs_file_alloc (void);
void    vfs_file_get   (file_t* f);
void    vfs_file_put   (file_t* f);

// directory iteration (the only place that loops readdir)
typedef int64_t (*vfs_dir_cb)(dentry_t* entry, void* ctx);
int64_t vfs_iterate(file_t* f, vfs_dir_cb cb, void* ctx);
