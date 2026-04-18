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
        return FindIndirect(inode, block_idx);
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

    uint64_t dentry_namelen = strlen(dentry->name);

    for (uint32_t i = 0; i < dir_blocks; i++) {
        int64_t block_number = FindDataBlock(dir, i);
        if (block_number == 0) continue;

        ext2_dir_entry_t* entries = (ext2_dir_entry_t*) bread(dir->sb, block_number);
        uint32_t offset = 0;

        while (offset < dir->sb->block_size) {
            ext2_dir_entry_t* entry = (ext2_dir_entry_t*)((uint64_t)entries + offset);

            if (dentry_namelen != entry->name_len || entry->inode == 0) {
                offset += entry->rec_len;
                continue;
            }

            if (strncmp(entry->name, dentry->name, entry->name_len) == 0) {
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

uint32_t EXT2AddInode(superblock_t* sb, uint32_t block_group) {
    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;
    ext2_block_group_t* bg = &vol->bgdt[block_group];

    if (bg->free_inodes_count == 0) return 0;

    uint32_t inode_bitmap_block = bg->inode_bitmap;
    uint8_t* buf = (uint8_t*) bread(sb, inode_bitmap_block);

    uint32_t u64_inside_block = sb->block_size / sizeof(uint64_t);
    uint64_t* bitmap = (uint64_t*) buf;

    int64_t bit_idx = FindFirstUnsetInBuffer(bitmap, u64_inside_block);
    if (bit_idx == -1) {
        brelse(sb, inode_bitmap_block);
        return 0;
    }
    bitmap[bit_idx / 64] |= (1ULL << (bit_idx % 64));
    bwrite(sb, inode_bitmap_block);
    brelse(sb, inode_bitmap_block);

    vol->free_inodes--;
    vol->bgdt[block_group].free_inodes_count--;

    return block_group * vol->inodes_per_group + bit_idx + 1;
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

    uint32_t block_idx = inode->size / inode->sb->block_size;

    data->i_blocks += inode->sb->block_size / 512;

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

int64_t EXT2FindLastDirEntryLocation(inode_t* dir, uint32_t* out_block_idx, uint32_t* out_block_offset) {
    uint32_t block_size = dir->sb->block_size;
    uint32_t dir_blocks = dir->size / block_size;

    if (dir_blocks == 0) {
        *out_block_idx = 0;
        *out_block_offset = 0;
        return -1;
    }
    int64_t block_number = FindDataBlock(dir, dir_blocks - 1);

    ext2_dir_entry_t* entries = (ext2_dir_entry_t*) bread(dir->sb, block_number);
    uint32_t offset = 0;

    while (offset < block_size) {
        ext2_dir_entry_t* entry = (ext2_dir_entry_t*)((uint64_t)entries + offset);

        if (offset + entry->rec_len == block_size) {
            *out_block_idx = block_number;
            *out_block_offset = offset;
            brelse(dir->sb, block_number);
            return 0;
        } 
        offset += entry->rec_len;
    }
    brelse(dir->sb, block_number);
    return -2;
}

int64_t EXT2PopulateDirEntry(inode_t* dir, dentry_t* dentry, uint64_t type) {
    ext2_dir_entry_t new_entry;
    total_time_t curr_total_time;
    GetTotalTime(&curr_total_time);
    ((ext2_inode_data_t*)dir->fs_specific)->accessed_at = CalculateUnixTimestamp(&curr_total_time);

    uint32_t name_len = strlen(dentry->name);

    uint32_t req_space = sizeof(ext2_dir_entry_t) + name_len;
    uint32_t last_block_idx, last_bytes_offset;

    int64_t last_res = EXT2FindLastDirEntryLocation(dir, &last_block_idx, &last_bytes_offset);
    if (last_res < 0 && last_res != -1) return last_res;

    if (last_res == -1) {
        // An empty dir, we need to allocate the first block
        last_block_idx = EXT2AllocBlock(dir->sb, ((ext2_inode_data_t*)dir->fs_specific)->block_group);
        if (last_block_idx == 0) return -ENOSPC;
        EXT2AddBlockToInode(dir, last_block_idx);
        last_bytes_offset = 0;
        dir->size += dir->sb->block_size;

        new_entry = (ext2_dir_entry_t) {
            .inode = ((ext2_inode_data_t*)dentry->inode->fs_specific)->inode_number,
            .rec_len = dir->sb->block_size,
            .name_len = name_len,
            .file_type = EXT2TypeToFT(type)
         };
        ext2_dir_entry_t* new_entry_ptr = (ext2_dir_entry_t*) bread(dir->sb, last_block_idx);
        *new_entry_ptr = new_entry;
        memcpy((void*)(new_entry_ptr->name), dentry->name, name_len);

        bwrite(dir->sb, last_block_idx);
        brelse(dir->sb, last_block_idx);

        dir->sb->ops->write_inode(dir);
        return 0;
    }

    ext2_dir_entry_t* last_entry = (ext2_dir_entry_t*) ((uint64_t)bread(dir->sb, last_block_idx) + last_bytes_offset);

    uint32_t last_entry_actual_size = EXT2_DIRENT_ALIGN(sizeof(ext2_dir_entry_t) + last_entry->name_len);
    uint32_t free_space = last_entry->rec_len - last_entry_actual_size;
    uint32_t new_entry_block, new_entry_offset;

    if (free_space < req_space) {
        new_entry_block = EXT2AllocBlock(dir->sb, ((ext2_inode_data_t*)dir->fs_specific)->block_group);
        if (new_entry_block == 0) return -ENOSPC;

        EXT2AddBlockToInode(dir, new_entry_block);

        new_entry_offset = 0;
        dir->size += dir->sb->block_size;
    }
    else {
        new_entry_block = last_block_idx;
        new_entry_offset = last_bytes_offset + last_entry_actual_size;

        last_entry->rec_len = last_entry_actual_size;
        bwrite(dir->sb, last_block_idx);
        brelse(dir->sb, last_block_idx);
    }

    GetTotalTime(&curr_total_time);
    ((ext2_inode_data_t*)dir->fs_specific)->modified_at = CalculateUnixTimestamp(&curr_total_time);

    new_entry = (ext2_dir_entry_t) {
        .inode = ((ext2_inode_data_t*)dentry->inode->fs_specific)->inode_number,
        .rec_len = dir->sb->block_size - new_entry_offset,
        .name_len = name_len,
        .file_type = EXT2TypeToFT(type)
    };

    ext2_dir_entry_t* new_entry_ptr = (ext2_dir_entry_t*) ((uint64_t)bread(dir->sb, new_entry_block) + new_entry_offset);
    *new_entry_ptr = new_entry;
    memcpy((void*)(new_entry_ptr->name), dentry->name, name_len);
    bwrite(dir->sb, new_entry_block);
    brelse(dir->sb, new_entry_block);

    dir->sb->ops->write_inode(dir);
    return 0;
}


int64_t EXT2PopulateNewInode(inode_t* inode, uint64_t type, uint64_t permissions) {
    if (inode == NULL) return -1;
    if (inode->fs_specific == NULL) return -ENOMEM;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;

    data->inode_number = EXT2AddInode(inode->sb, data->block_group);
    if (data->inode_number == 0) return -ENOSPC;

    inode->type = type;
    inode->permissions = permissions;
    inode->size = 0;
    inode->owner_id = 0; // TODO: set real owner
    inode->group_id = 0; // TODO: set real group

    
    data->block_group = EXT2InodeNumberToGroup(vol, data->inode_number);
    data->i_blocks = 0;
    data->ref_count = (type == VFS_TYPE_DIR) ? 2 : 1;

    total_time_t curr_total_time;
    GetTotalTime(&curr_total_time);
    uint64_t curr_time = CalculateUnixTimestamp(&curr_total_time);

    data->accessed_at = curr_time;
    data->modified_at = curr_time;
    data->changed_at = curr_time;

    inode->sb->ops->write_inode(inode);

    return 0;
}

int64_t EXT2PopulateInodeEntry(inode_t* dir, inode_t** out, uint64_t type, uint64_t permissions) {
    inode_t* inode = dir->sb->ops->alloc_inode(dir->sb);
    if (inode == NULL) return -ENOMEM;

    ext2_inode_data_t* data = inode->fs_specific;
    data->block_group = ((ext2_inode_data_t*)dir->fs_specific)->block_group;

    int64_t res_pop = EXT2PopulateNewInode(inode, type, permissions);
    if (res_pop < 0) return res_pop;

    *out = inode;
    return 0;
}
int64_t EXT2CreateGeneric(inode_t* dir, dentry_t* dentry, uint64_t permissions, uint64_t type) {
    int64_t inode_entry_res = EXT2PopulateInodeEntry(dir, &dentry->inode, type, permissions);
    if (inode_entry_res < 0) return inode_entry_res;

    int64_t dir_entry_res = EXT2PopulateDirEntry(dir, dentry, type);
    if (dir_entry_res < 0) return dir_entry_res;

    return 0;
}

int64_t EXT2Create(inode_t* dir, dentry_t* dentry, uint64_t permissions) {
    return EXT2CreateGeneric(dir, dentry, permissions, VFS_TYPE_FILE);
}

int64_t EXT2Mkdir(inode_t* dir, dentry_t* dentry, uint64_t permissions) {
    int64_t res = EXT2CreateGeneric(dir, dentry, permissions, VFS_TYPE_DIR);
    if (res < 0) return res;

    // Add . and .. entries to the new directory
    dentry_t dot = {
        .name = ".",
        .inode = dentry->inode
    };
    dentry_t dotdot = {
        .name = "..",
        .inode = dir
    };
    int64_t dot_res = EXT2PopulateDirEntry(dentry->inode, &dot, VFS_TYPE_DIR);
    if (dot_res < 0) return dot_res;

    int64_t dotdot_res = EXT2PopulateDirEntry(dentry->inode, &dotdot, VFS_TYPE_DIR);
    if (dotdot_res < 0) return dotdot_res;

    // The .. entry is a hard link to the parent; increment parent's link count
    ((ext2_inode_data_t*)dir->fs_specific)->ref_count++;
    dir->sb->ops->write_inode(dir);

    // Track the new directory in the block group descriptor
    ext2_info_t* vol = (ext2_info_t*) dir->sb->fs_info;
    uint32_t bg = ((ext2_inode_data_t*)dentry->inode->fs_specific)->block_group;
    vol->bgdt[bg].used_dirs_count++;

    return 0;
}

int64_t EXT2DeleteInodeFromBG(inode_t* inode) {
    if (inode == NULL) return -1;
    if (inode->fs_specific == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*)inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    if (data->inode_number == 0) return -1;

    ext2_block_group_t* bg = &vol->bgdt[data->block_group];

    uint64_t* bitmap = (uint64_t*) bread(inode->sb, bg->inode_bitmap);

    uint32_t inode_idx_inside_bitmap = (data->inode_number - 1) - vol->inodes_per_group * data->block_group;
    uint64_t u64_idx = inode_idx_inside_bitmap / 64, bit_idx = inode_idx_inside_bitmap % 64;

    bitmap[u64_idx] &= ~(1 << bit_idx);
    bwrite(inode->sb, bg->inode_bitmap);
    brelse(inode->sb, bg->inode_bitmap);

    bg->free_inodes_count++;
    return 0;
}

int64_t EXT2DeleteBlockFromBG(inode_t* inode, uint64_t block_idx) {
    if (inode == NULL || block_idx == 0) return -1;
    if (inode->fs_specific == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*)inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    if (data->inode_number == 0) return -1;

    uint32_t bg_idx = block_idx / vol->blocks_per_group;
    ext2_block_group_t* bg = &vol->bgdt[bg_idx];
    bg->free_blocks_count++;
    
    uint64_t* bitmap = (uint64_t*) bread(inode->sb, bg->block_bitmap);

    uint32_t block_idx_inside_bitmap = (block_idx - vol->first_data_block) - vol->blocks_per_group * bg_idx;
    uint64_t u64_idx = block_idx_inside_bitmap / 64, bit_idx = block_idx_inside_bitmap % 64;

    bitmap[u64_idx] &= ~(1 << bit_idx);
    bwrite(inode->sb, bg->block_bitmap);
    brelse(inode->sb, bg->block_bitmap);

    return 0;
}

int64_t EXT2DeleteDataBlocksFromBG(inode_t* inode) {
    if (inode == NULL) return -1;
    if (inode->fs_specific == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*)inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    if (data->inode_number == 0) return -1;

    ext2_block_group_t* bg = &vol->bgdt[data->block_group];

    uint64_t* bitmap = (uint64_t*) bread(inode->sb, bg->block_bitmap);
    uint64_t data_blocks_num = (inode->size + inode->sb->block_size - 1) / inode->sb->block_size, data_block;

    for (uint64_t i = 0; i < data_blocks_num; i++) {
        data_block = FindDataBlock(inode, i);
        EXT2DeleteBlockFromBG(inode, data_block);
    }
}

int64_t SetDeleteTime(inode_t* inode) {
    total_time_t t;
    GetTotalTime(&t);

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    data->i_dtime = CalculateUnixTimestamp(&t);
    return 0;
}

int64_t EXT2Unlink(inode_t* dir, dentry_t* dentry) {
    if (dir == NULL || dentry == NULL) return -1;
    if (dir->fs_specific == NULL || dentry->name == NULL) return -1;

    ext2_inode_data_t* data = (ext2_inode_data_t*)dir->fs_specific;
    data->ref_count--;
    
    uint32_t dir_blocks = dir->size / dir->sb->block_size;
    uint32_t inode_number;

    for (uint32_t i = 0; i < dir_blocks; i++) {
        int64_t block_number = FindDataBlock(dir, i);
        if (block_number == 0) continue;

        ext2_dir_entry_t* entries = (ext2_dir_entry_t*) bread(dir->sb, block_number), *p1, *p2;
        p1 = (ext2_dir_entry_t*)((uint64_t)entries);
        
        if (strncmp(p1->name, dentry->name, p1->name_len) == 0) {
            p1->inode = 0;
            brelse(dir->sb, block_number);
            goto end_of_nested;
        }

        uint32_t offset = 0;

        while (offset + p1->rec_len < dir->sb->block_size) {
            
            p2 = (ext2_dir_entry_t*)((uint64_t)p1 + p1->rec_len);

            if (p2->inode != 0 && strncmp(p2->name, dentry->name, p2->name_len) == 0) {
                p1->rec_len += p2->rec_len;
                inode_number = p2->inode;

                bwrite(dir->sb, block_number);
                brelse(dir->sb, block_number);
                
                goto end_of_nested;
            }
            offset += p1->rec_len;
            p1 = (ext2_dir_entry_t*)((uint64_t)entries + offset);

        }

        brelse(dir->sb, block_number);
    }
    end_of_nested:

    if (inode_number == 0) return 0;

    inode_t* del = dir->sb->ops->alloc_inode(dir->sb);
    ext2_inode_data_t* del_data = (ext2_inode_data_t*)del->fs_specific;
    del_data->inode_number = inode_number;
    dir->sb->ops->read_inode(del);
    del_data->ref_count--;

    if (del_data->ref_count == 0) {
        EXT2DeleteInodeFromBG(del);
        EXT2DeleteDataBlocksFromBG(del);
        SetDeleteTime(del);
        del->sb->ops->write_inode(del);
        del->sb->ops->free_inode(del);
    }

    return 0;
}