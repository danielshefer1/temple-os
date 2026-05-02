#pragma once

#include "types.h"
#include "defintions.h"
#include "extern.h"
#include "paging.h"
#include "slab_alloc.h"
#include "mutex.h"

void* bread(superblock_t* sb, uint32_t block_number);
void brelse(superblock_t* sb, uint32_t block_number);
void bwrite(superblock_t* sb, uint32_t block_number);
void bflush(superblock_t* sb, uint32_t block_number);
void bflush_all(superblock_t* sb);
void bclean(superblock_t* sb, uint32_t block_number);
void bclean_all(superblock_t* sb);
void binvalidate(superblock_t* sb, uint32_t block_number);