#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vfs.h"
#include "paging.h"
#include "string.h"
#include "slab_alloc.h"

superblock_t* Fat32MountRootWrapper();
int64_t Fat32MountRoot(superblock_t* sb);
int64_t Fat32Mount(superblock_t* sb);
int64_t Fat32_LookUp(inode_t* parent_dir, dentry_t* dentry);
void PrintDirEntry(fat32_dir_entry_t* entry, char* name);