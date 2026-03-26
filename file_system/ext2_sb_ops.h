#pragma once

#include "types.h"
#include "includes.h"
#include "global.h"
#include "buddy_alloc.h"
#include "slab_alloc.h"
#include "string.h"
#include "rtc.h"

superblock_t* EXT2MountRoot();
int64_t EXT2FindRoot(superblock_t* sb);
int64_t EXT2Mount(superblock_t* sb);
int64_t CopySbExtToInternal(ext2_superblock_disk_t* sbext, superblock_t* sb);
int64_t EXT2ReadBlocks(superblock_t* sb, uint32_t block_idx, uint32_t count, void* buf);