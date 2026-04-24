#include "ext2_helpers.h"

uint64_t EXT2ModeToType(uint16_t mode) {
    switch (mode & 0xF000) {
        case EXT2_S_IFREG: return VFS_TYPE_FILE;
        case EXT2_S_IFDIR: return VFS_TYPE_DIR;
        case EXT2_S_IFLNK: return VFS_TYPE_SYMLINK;
        case EXT2_S_IFCHR: return VFS_TYPE_CHARDEV;
        case EXT2_S_IFBLK: return VFS_TYPE_BLOCKDEV;
        case EXT2_S_IFIFO: return VFS_TYPE_FIFO;
        case EXT2_S_IFSOCK: return VFS_TYPE_SOCKET;
        default:     return VFS_TYPE_UNKNOWN;
    }
}

uint32_t EXT2TypeToMode(uint64_t type) {
    switch (type) {
        case VFS_TYPE_FILE: return EXT2_S_IFREG;
        case VFS_TYPE_DIR: return EXT2_S_IFDIR;
        case VFS_TYPE_SYMLINK: return EXT2_S_IFLNK;
        case VFS_TYPE_CHARDEV: return EXT2_S_IFCHR;
        case VFS_TYPE_BLOCKDEV: return EXT2_S_IFBLK;
        case VFS_TYPE_FIFO: return EXT2_S_IFIFO;
        case VFS_TYPE_SOCKET: return EXT2_S_IFSOCK;
        default:     return 0;
    }
}

uint8_t EXT2TypeToFT(uint64_t type) {
    switch (type) {
        case VFS_TYPE_FILE: return EXT2_FT_REG_FILE;
        case VFS_TYPE_DIR: return EXT2_FT_DIR;
        case VFS_TYPE_SYMLINK: return EXT2_FT_SYMLINK;
        case VFS_TYPE_CHARDEV: return EXT2_FT_CHRDEV;
        case VFS_TYPE_BLOCKDEV: return EXT2_FT_BLKDEV;
        case VFS_TYPE_FIFO: return EXT2_FT_FIFO;
        case VFS_TYPE_SOCKET: return EXT2_FT_SOCK;
        default:     return EXT2_FT_UNKNOWN;
    }
}

uint32_t EXT2InodeNumberToGroup(ext2_info_t* vol, uint32_t inode_number) {
    return (inode_number - 1) / vol->inodes_per_group;
}

uint32_t EXT2PremissionsToMod(uint64_t permissions) {
    return permissions & 0xFFF;
}

uint64_t EXT2FlagsToVFSFlags(uint32_t i_flags) {
    uint64_t flags = 0;
    if (i_flags & EXT2_IMMUTABLE_FL) flags |= S_IMMUTABLE;
    if (i_flags & EXT2_APPEND_FL) flags |= S_APPEND;
    if (i_flags & EXT2_NOATIME_FL) flags |= S_NOATIME;
    if (i_flags & EXT2_NODUMP_FL) flags |= S_NODUMP;
    if (i_flags & EXT2_SYNC_FL) flags |= S_SYNC;

    return flags;
}

uint32_t EXT2VFSFlagsToIFlags(uint64_t flags) {
    uint32_t i_flags = 0;
    if (flags & S_IMMUTABLE) i_flags |= EXT2_IMMUTABLE_FL;
    if (flags & S_APPEND) i_flags |= EXT2_APPEND_FL;
    if (flags & S_NOATIME) i_flags |= EXT2_NOATIME_FL;
    if (flags & S_NODUMP) i_flags |= EXT2_NODUMP_FL;
    if (flags & S_SYNC) i_flags |= EXT2_SYNC_FL;

    return i_flags;
}

int64_t FindIndirect(inode_t* inode, uint32_t block_idx) {
    if (inode == NULL) return -1;
    if (inode->fs_specific == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    uint32_t indirect_block = data->i_block[12];
    if (indirect_block == 0) return 0;

    uint32_t* buf = (uint32_t*) bread(inode->sb, indirect_block);
    uint32_t block = (block_idx - 12) % EXT2_BLOCKS_PER_BLOCK(inode->sb);
    uint32_t ret = buf[block];
        
    brelse(inode->sb, indirect_block);
    if (ret == 0) return 0;
    return ret;
}

int64_t FindDoubleIndirect(inode_t* inode, uint32_t block_idx) {
    if (inode == NULL) return 0;
    if (inode->fs_specific == NULL) return 0;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    uint32_t double_indirect_block = data->i_block[13];

    uint32_t* buf = (uint32_t*) bread(inode->sb, double_indirect_block);
    uint32_t indirect_block = buf[(block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb)) / EXT2_BLOCKS_PER_BLOCK(inode->sb)];
    if (indirect_block == 0) {
        brelse(inode->sb, double_indirect_block);
        return 0;
    }

    uint32_t* buf2 = (uint32_t*) bread(inode->sb, indirect_block);
    uint32_t block = buf2[(block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb)) % EXT2_BLOCKS_PER_BLOCK(inode->sb)];
    brelse(inode->sb, double_indirect_block);
    brelse(inode->sb, indirect_block);

    return block;
}

int64_t FindTripleIndirect(inode_t* inode, uint32_t block_idx) {
    if (inode == NULL) return 0;
    if (inode->fs_specific == NULL) return 0;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    uint32_t triple_indirect_block = data->i_block[14];
    if (triple_indirect_block == 0) return 0;

    uint32_t* buf = (uint32_t*) bread(inode->sb, triple_indirect_block);
    uint32_t double_indirect_block = buf[(block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb) - EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb)) / (EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb))];
    if (double_indirect_block == 0) {
        brelse(inode->sb, triple_indirect_block);
        return 0;
    }

    uint32_t* buf2 = (uint32_t*) bread(inode->sb, double_indirect_block);
    uint32_t indirect_block = buf2[((block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb) - EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb)) / EXT2_BLOCKS_PER_BLOCK(inode->sb)) % EXT2_BLOCKS_PER_BLOCK(inode->sb)];
    if (indirect_block == 0) {
        brelse(inode->sb, triple_indirect_block);
        brelse(inode->sb, double_indirect_block);
        return 0;
    }

    uint32_t* buf3 = (uint32_t*) bread(inode->sb, indirect_block);
    uint32_t block = buf3[(block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb) - EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb)) % EXT2_BLOCKS_PER_BLOCK(inode->sb)];
    
    brelse(inode->sb, triple_indirect_block);
    brelse(inode->sb, double_indirect_block);
    brelse(inode->sb, indirect_block);

    return block;
}

int64_t FindDataBlock(inode_t* inode, uint32_t block_idx) {
    if (inode == NULL) return -1;
    if (inode->fs_specific == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    if (block_idx < 12) {
        return data->i_block[block_idx];
    }
    if (block_idx < 12 + EXT2_BLOCKS_PER_BLOCK(inode->sb)) {
        return FindIndirect(inode, block_idx);
    }
    if (block_idx < 12 + EXT2_BLOCKS_PER_BLOCK(inode->sb) + EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb)) {
        return FindDoubleIndirect(inode, block_idx);
    }
    return FindTripleIndirect(inode, block_idx);
}

uint32_t EXT2AllocBlock(superblock_t* sb, uint32_t block_group) {
    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;
    ext2_block_group_t* bg = &vol->bgdt[block_group];

    if (bg->free_blocks_count == 0) return 0;

    uint32_t block_bitmap_block = bg->block_bitmap;
    uint8_t* buf = (uint8_t*) bread(sb, block_bitmap_block);

    uint32_t u64_inside_block = sb->block_size / sizeof(uint64_t);
    uint64_t* bitmap = (uint64_t*) buf;

    int64_t bit_idx = FindFirstUnsetInBuffer(bitmap, u64_inside_block);
    if (bit_idx == -1) {
        brelse(sb, block_bitmap_block);
        return 0;
    }
    vol->free_blocks--;
    vol->bgdt[block_group].free_blocks_count--;

    bitmap[bit_idx / 64] |= (1ULL << (bit_idx % 64));
    bwrite(sb, block_bitmap_block);
    brelse(sb, block_bitmap_block);

    return block_group * vol->blocks_per_group + bit_idx + vol->first_data_block + 1;
}

int64_t AddIndirect(inode_t* inode, uint32_t block_idx, uint32_t block_number) {
    if (inode == NULL) return -1;
    if (inode->fs_specific == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    uint32_t indirect_block = data->i_block[12];
    if (indirect_block == 0) return 0;

    uint32_t* buf = (uint32_t*) bread(inode->sb, indirect_block);
    uint32_t block = (block_idx - 12) % EXT2_BLOCKS_PER_BLOCK(inode->sb);
    buf[block] = block_number;
    bwrite(inode->sb, indirect_block);  
    brelse(inode->sb, indirect_block);
    
    return 0;
}

int64_t AddDoubleIndirect(inode_t* inode, uint32_t block_idx, uint32_t block_number) {
    if (inode == NULL) return 0;
    if (inode->fs_specific == NULL) return 0;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    uint32_t double_indirect_block = data->i_block[13];

    uint32_t indirect_idx = (block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb)) / EXT2_BLOCKS_PER_BLOCK(inode->sb);
    uint32_t double_indirect_idx = (block_idx - 12 - EXT2_BLOCKS_PER_BLOCK(inode->sb)) % EXT2_BLOCKS_PER_BLOCK(inode->sb);

    uint32_t* buf = (uint32_t*) bread(inode->sb, double_indirect_block);
    uint32_t indirect_block = buf[indirect_idx];

    if (indirect_block == 0) {
        indirect_block = EXT2AllocBlock(inode->sb, ((ext2_inode_data_t*)inode->fs_specific)->block_group);
        buf[indirect_idx] = indirect_block;
        bwrite(inode->sb, double_indirect_block);
    }

    uint32_t* buf2 = (uint32_t*) bread(inode->sb, indirect_block);
    buf2[double_indirect_idx] = block_number;
    bwrite(inode->sb, indirect_block);

    brelse(inode->sb, double_indirect_block);
    brelse(inode->sb, indirect_block);

    return 0;
}

int64_t AddTripleIndirect(inode_t* inode, uint32_t block_idx, uint32_t block_number) {
    if (inode == NULL) return 0;
    if (inode->fs_specific == NULL) return 0;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    uint32_t triple_indirect_block = data->i_block[14];
    if (triple_indirect_block == 0) return 0;

    uint32_t blocks_per_block = EXT2_BLOCKS_PER_BLOCK(inode->sb);

    uint32_t indirect_idx = (block_idx - 12 - blocks_per_block - blocks_per_block * blocks_per_block) % blocks_per_block;
    uint32_t double_indirect_idx = ((block_idx - 12 - blocks_per_block - blocks_per_block * blocks_per_block) / blocks_per_block) % blocks_per_block;
    uint32_t triple_indirect_idx = (block_idx - 12 - blocks_per_block - blocks_per_block * blocks_per_block) / (blocks_per_block * blocks_per_block);

    uint32_t* buf = (uint32_t*) bread(inode->sb, triple_indirect_block);
    uint32_t double_indirect_block = buf[triple_indirect_idx];
    if (double_indirect_block == 0) {
        double_indirect_block = EXT2AllocBlock(inode->sb, ((ext2_inode_data_t*)inode->fs_specific)->block_group);
        buf[triple_indirect_idx] = double_indirect_block;
        bwrite(inode->sb, triple_indirect_block);
    }

    uint32_t* buf2 = (uint32_t*) bread(inode->sb, double_indirect_block);
    uint32_t indirect_block = buf2[double_indirect_idx];
    if (indirect_block == 0) {
        indirect_block = EXT2AllocBlock(inode->sb, ((ext2_inode_data_t*)inode->fs_specific)->block_group);
        buf2[double_indirect_idx] = indirect_block;
        bwrite(inode->sb, double_indirect_block);
    }

    uint32_t* buf3 = (uint32_t*) bread(inode->sb, indirect_block);
    buf3[indirect_idx] = block_number;
    bwrite(inode->sb, indirect_block);

    brelse(inode->sb, triple_indirect_block);
    brelse(inode->sb, double_indirect_block);
    brelse(inode->sb, indirect_block);

    return 0;
}

uint32_t EXT2AddBlockToInode(inode_t* inode, uint32_t block_number) {
    if (inode == NULL) return 0;
    if (inode->fs_specific == NULL) return 0;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;

    uint32_t block_idx = (data->i_blocks * inode->sb->bdev->sector_size) / inode->sb->block_size;

    data->i_blocks += inode->sb->block_size / inode->sb->bdev->sector_size;

    if (block_idx < 12) {
        data->i_block[block_idx] = block_number;
        inode->sb->ops->write_inode(inode);
        return 0;
    }
    if (block_idx < 12 + EXT2_BLOCKS_PER_BLOCK(inode->sb)) {
        int64_t res = AddIndirect(inode, block_idx, block_number);
        if (res < 0) return res;

        inode->sb->ops->write_inode(inode);
        return 0;
    }
    if (block_idx < 12 + EXT2_BLOCKS_PER_BLOCK(inode->sb) + EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb)) {
        int64_t res = AddDoubleIndirect(inode, block_idx, block_number);
        if (res < 0) return res;

        inode->sb->ops->write_inode(inode);
        return 0;
    }
    int64_t res = AddTripleIndirect(inode, block_idx, block_number);
    if (res < 0) return res;

    inode->sb->ops->write_inode(inode);
    return 0;
}

static int FreeInSingleIndirect(inode_t* inode, uint32_t indirect_block, uint64_t start) {
    uint64_t bpb = EXT2_BLOCKS_PER_BLOCK(inode->sb);
    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    uint32_t sectors_per_block = inode->sb->block_size / 512;

    uint32_t* buf = (uint32_t*) bread(inode->sb, indirect_block);
    if (buf == NULL) return -EIO;

    for (uint64_t j = start; j < bpb; j++) {
        if (buf[j] == 0) continue;
        EXT2DeleteBlockFromBG(inode, buf[j]);
        binvalidate(inode->sb, buf[j]);
        buf[j] = 0;
        if (data->i_blocks >= sectors_per_block) data->i_blocks -= sectors_per_block;
    }

    int fully_empty = 1;
    for (uint64_t j = 0; j < bpb; j++) {
        if (buf[j] != 0) { fully_empty = 0; break; }
    }

    bwrite(inode->sb, indirect_block);
    brelse(inode->sb, indirect_block);
    return fully_empty;
}

static int FreeInDoubleIndirect(inode_t* inode, uint32_t double_block, uint64_t start_outer, uint64_t start_inner) {
    uint64_t bpb = EXT2_BLOCKS_PER_BLOCK(inode->sb);

    uint32_t* buf = (uint32_t*) bread(inode->sb, double_block);
    if (buf == NULL) return -EIO;

    for (uint64_t i = start_outer; i < bpb; i++) {
        if (buf[i] == 0) continue;
        uint64_t inner = (i == start_outer) ? start_inner : 0;
        int empty = FreeInSingleIndirect(inode, buf[i], inner);
        if (empty == 1) {
            EXT2DeleteBlockFromBG(inode, buf[i]);
            binvalidate(inode->sb, buf[i]);
            buf[i] = 0;
        }
    }

    int fully_empty = 1;
    for (uint64_t j = 0; j < bpb; j++) {
        if (buf[j] != 0) { fully_empty = 0; break; }
    }

    bwrite(inode->sb, double_block);
    brelse(inode->sb, double_block);
    return fully_empty;
}

static int FreeInTripleIndirect(inode_t* inode, uint32_t triple_block, uint64_t start_triple, uint64_t start_outer, uint64_t start_inner) {
    uint64_t bpb = EXT2_BLOCKS_PER_BLOCK(inode->sb);

    uint32_t* buf = (uint32_t*) bread(inode->sb, triple_block);
    if (buf == NULL) return -EIO;

    for (uint64_t i = start_triple; i < bpb; i++) {
        if (buf[i] == 0) continue;
        uint64_t outer = (i == start_triple) ? start_outer : 0;
        uint64_t inner = (i == start_triple) ? start_inner : 0;
        int empty = FreeInDoubleIndirect(inode, buf[i], outer, inner);
        if (empty == 1) {
            EXT2DeleteBlockFromBG(inode, buf[i]);
            binvalidate(inode->sb, buf[i]);
            buf[i] = 0;
        }
    }

    int fully_empty = 1;
    for (uint64_t j = 0; j < bpb; j++) {
        if (buf[j] != 0) { fully_empty = 0; break; }
    }

    bwrite(inode->sb, triple_block);
    brelse(inode->sb, triple_block);
    return fully_empty;
}

int64_t EXT2FreeBlocksFrom(inode_t* inode, uint64_t first_logical_idx) {
    if (inode == NULL || inode->fs_specific == NULL) return -EINVAL;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    uint64_t bpb = EXT2_BLOCKS_PER_BLOCK(inode->sb);
    uint32_t sectors_per_block = inode->sb->block_size / 512;

    uint64_t single_start = 12;
    uint64_t double_start = 12 + bpb;
    uint64_t triple_start = 12 + bpb + bpb * bpb;

    if (first_logical_idx < single_start) {
        for (uint64_t i = first_logical_idx; i < 12; i++) {
            if (data->i_block[i] == 0) continue;
            EXT2DeleteBlockFromBG(inode, data->i_block[i]);
            binvalidate(inode->sb, data->i_block[i]);
            data->i_block[i] = 0;
            if (data->i_blocks >= sectors_per_block) data->i_blocks -= sectors_per_block;
        }
    }

    if (first_logical_idx < double_start && data->i_block[12] != 0) {
        uint64_t inner = (first_logical_idx > single_start) ? (first_logical_idx - single_start) : 0;
        int empty = FreeInSingleIndirect(inode, data->i_block[12], inner);
        if (empty == 1) {
            EXT2DeleteBlockFromBG(inode, data->i_block[12]);
            binvalidate(inode->sb, data->i_block[12]);
            data->i_block[12] = 0;
        }
    }

    if (first_logical_idx < triple_start && data->i_block[13] != 0) {
        uint64_t offset = (first_logical_idx > double_start) ? (first_logical_idx - double_start) : 0;
        uint64_t outer = offset / bpb;
        uint64_t inner = offset % bpb;
        int empty = FreeInDoubleIndirect(inode, data->i_block[13], outer, inner);
        if (empty == 1) {
            EXT2DeleteBlockFromBG(inode, data->i_block[13]);
            binvalidate(inode->sb, data->i_block[13]);
            data->i_block[13] = 0;
        }
    }

    if (data->i_block[14] != 0) {
        uint64_t offset = (first_logical_idx > triple_start) ? (first_logical_idx - triple_start) : 0;
        uint64_t triple = offset / (bpb * bpb);
        uint64_t rem = offset % (bpb * bpb);
        uint64_t outer = rem / bpb;
        uint64_t inner = rem % bpb;
        int empty = FreeInTripleIndirect(inode, data->i_block[14], triple, outer, inner);
        if (empty == 1) {
            EXT2DeleteBlockFromBG(inode, data->i_block[14]);
            binvalidate(inode->sb, data->i_block[14]);
            data->i_block[14] = 0;
        }
    }

    return 0;
}