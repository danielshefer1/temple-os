#pragma once

#include "types.h"

inode_t* vfs_alloc_inode (superblock_t* sb);
int64_t  vfs_free_inode  (inode_t* in);
int64_t  vfs_read_inode  (inode_t* in);
int64_t  vfs_write_inode (inode_t* in);

int64_t  vfs_mount       (superblock_t* sb);
int64_t  vfs_unmount     (superblock_t* sb);
int64_t  vfs_sync        (superblock_t* sb);

int64_t  vfs_stat        (superblock_t* sb, fs_stat_t* out);
