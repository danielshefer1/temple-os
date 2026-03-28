#pragma once

#include "types.h"
#include "includes.h"
#include "global.h"
#include "buddy_alloc.h"
#include "slab_alloc.h"
#include "string.h"
#include "rtc.h"

int64_t EXT2ReadBlocks(superblock_t* sb, uint32_t block_idx, uint32_t count, void* buf);


superblock_t* EXT2MountRoot();
int64_t EXT2FindRoot(superblock_t* sb);
int64_t EXT2Mount(superblock_t* sb);

int64_t CopySbExtToInternal(ext2_superblock_disk_t* sbext, superblock_t* sb);
int64_t EXT2Umount(superblock_t* sb);

int64_t EXT2WriteBlocks(superblock_t* sb, uint32_t block_idx, uint32_t count, void* buf);

inode_t* EXT2AllocInode(superblock_t* sb);
int64_t EXT2ReadInode(inode_t* inode);