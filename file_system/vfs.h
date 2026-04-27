#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "memory.h"
#include "vga.h"
#include "string.h"
#include "dcache.h"

// ---- shared check helpers (used by every vfs_* file; no duplication) ----
int64_t vfs_check_sb       (superblock_t* sb);
int64_t vfs_check_inode    (inode_t* in);
int64_t vfs_check_dir      (inode_t* dir);
int64_t vfs_check_file     (file_t* f);
int64_t vfs_check_dentry   (dentry_t* d);
int64_t vfs_check_writable (inode_t* in);

// dispatch macro: call op or return -ENOTSUP if not implemented
#define VFS_CALL(ops, fn, ...) ((ops)->fn ? (ops)->fn(__VA_ARGS__) : -ENOTSUP)

// duplicate a NUL-terminated string into a kmalloc'd buffer
char* vfs_strdup(const char* s);
void  vfs_strfree(char* s);

// umbrella includes
#include "vfs_sb.h"
#include "vfs_inode.h"
#include "vfs_file.h"
#include "vfs_dentry.h"
#include "vfs_mount.h"
#include "vfs_path.h"
#include "vfs_path_ops.h"
