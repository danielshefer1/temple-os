#include "ext2_ino_ops.h"

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
        FindIndirect(inode, block_idx);
    }
    if (block_idx < 12 + EXT2_BLOCKS_PER_BLOCK(inode->sb) + EXT2_BLOCKS_PER_BLOCK(inode->sb) * EXT2_BLOCKS_PER_BLOCK(inode->sb)) {
        return FindDoubleIndirect(inode, block_idx);
    }
    return FindTripleIndirect(inode, block_idx);
}

int64_t EXT2Lookup(inode_t* dir, dentry_t* dentry) {
    if (dir == NULL || dentry == NULL) return -1;
    if (dir->fs_specific == NULL || dentry->name == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*)dir->fs_specific;
    
    uint32_t dir_blocks = dir->size / dir->sb->block_size;

    for (uint32_t i = 0; i < dir_blocks; i++) {
        int64_t block_number = FindDataBlock(dir, i);
        if (block_number == 0) continue;

        ext2_dir_entry_t* entries = (ext2_dir_entry_t*) bread(dir->sb, block_number);
        uint32_t offset = 0;

        while (offset < dir->sb->block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)((uint64_t)entries + offset);

            if (entry->inode != 0 && strncmp(entry->name, dentry->name, entry->name_len) == 0) {
                inode_t* inode = dir->sb->ops->alloc_inode(dir->sb);
                ext2_inode_data_t* inode_data = (ext2_inode_data_t*) inode->fs_specific;

                inode->fs_specific = inode_data;
                inode_data->inode_number = entry->inode;
                inode->sb->ops->read_inode(inode);

                dentry->inode = inode;
                brelse(dir->sb, block_number);
                return 0;
            }
            offset += entry->rec_len;
        }
        brelse(dir->sb, block_number);
    }
    return -1;
}