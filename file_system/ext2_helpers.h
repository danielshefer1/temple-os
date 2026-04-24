#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"

#include "blocks_buffer.h"

uint64_t EXT2ModeToType(uint16_t mode);
uint32_t EXT2TypeToMode(uint64_t type);
uint32_t EXT2InodeNumberToGroup(ext2_info_t* vol, uint32_t inode_number);
uint32_t EXT2PremissionsToMod(uint64_t permissions);
uint64_t EXT2FlagsToVFSFlags(uint32_t i_flags);
uint32_t EXT2VFSFlagsToIFlags(uint64_t flags);
uint8_t EXT2TypeToFT(uint64_t type);
int64_t FindDataBlock(inode_t* inode, uint32_t block_idx);
uint32_t EXT2AllocBlock(superblock_t* sb, uint32_t block_group);
uint32_t EXT2AddBlockToInode(inode_t* inode, uint32_t block_number);
int64_t EXT2DeleteBlockFromBG(inode_t* inode, uint64_t block_idx);
int64_t EXT2FreeBlocksFrom(inode_t* inode, uint64_t first_logical_idx);