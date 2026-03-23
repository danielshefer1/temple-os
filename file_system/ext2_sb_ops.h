#pragma once

#include "types.h"
#include "includes.h"
#include "global.h"
#include "buddy_alloc.h"
#include "string.h"

superblock_t* EXT2MountRoot();
int64_t EXT2FindRoot(superblock_t* sb);
int64_t EXT2Mount(superblock_t* sb);
int64_t CopySbExtToInternal(ext2_superblock_t* sbext, ext2_internal_info_t* vol);