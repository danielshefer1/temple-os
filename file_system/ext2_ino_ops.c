#include "ext2_ino_ops.h"



int64_t EXT2Lookup(inode_t* dir, dentry_t* dentry) {
    if (dir == NULL || dentry == NULL) return -1;
    if (dir->fs_specific == NULL || dentry->name == NULL) return -1;

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
    return -ENOENT;
}

uint32_t EXT2AddInode(superblock_t* sb, uint32_t block_group) {
    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;
    ext2_block_group_t* bg = &vol->bgdt[block_group];

    mutex_lock(&vol->inode_alloc_lock);

    if (bg->free_inodes_count == 0) {
        mutex_unlock(&vol->inode_alloc_lock);
        return 0;
    }

    uint32_t inode_bitmap_block = bg->inode_bitmap;
    uint8_t* buf = (uint8_t*) bread(sb, inode_bitmap_block);

    uint32_t u64_inside_block = sb->block_size / sizeof(uint64_t);
    uint64_t* bitmap = (uint64_t*) buf;

    int64_t bit_idx = FindFirstUnsetInBuffer(bitmap, u64_inside_block);
    if (bit_idx == -1) {
        brelse(sb, inode_bitmap_block);
        mutex_unlock(&vol->inode_alloc_lock);
        return 0;
    }
    bitmap[bit_idx / 64] |= (1ULL << (bit_idx % 64));
    bwrite(sb, inode_bitmap_block);
    brelse(sb, inode_bitmap_block);

    vol->free_inodes--;
    vol->bgdt[block_group].free_inodes_count--;

    mutex_unlock(&vol->inode_alloc_lock);
    return block_group * vol->inodes_per_group + bit_idx + 1;
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
    if (inode == NULL) return -EINVAL;
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
    if (EXT2Lookup(dir, dentry) == 0) return -EEXIST;

    int64_t inode_entry_res = EXT2PopulateInodeEntry(dir, &dentry->inode, type, permissions);
    if (inode_entry_res < 0) return inode_entry_res;

    int64_t dir_entry_res = EXT2PopulateDirEntry(dir, dentry, type);
    if (dir_entry_res < 0) return dir_entry_res;

    return 0;
}

int64_t EXT2Create(inode_t* dir, dentry_t* dentry, uint64_t permissions) {
    return EXT2CreateGeneric(dir, dentry, permissions, VFS_TYPE_FILE);
}

int64_t EXT2Mknod(inode_t* dir, dentry_t* dentry, uint64_t type,
                  uint64_t permissions, uint32_t dev_id) {
    if (type != VFS_TYPE_CHARDEV && type != VFS_TYPE_BLOCKDEV &&
        type != VFS_TYPE_FIFO    && type != VFS_TYPE_SOCKET) {
        return -EINVAL;
    }

    int64_t res = EXT2CreateGeneric(dir, dentry, permissions, type);
    if (res < 0) return res;

    // Stash the device id in i_block[0] using Linux's old encoding
    // (low byte = minor, next byte = major) — the same format
    // PopulateInode reads back. write_inode flushes it to disk.
    ext2_inode_data_t* data = (ext2_inode_data_t*) dentry->inode->fs_specific;
    data->i_block[0] = dev_id & 0xFFFFu;
    dentry->inode->dev_id = dev_id;
    dentry->inode->sb->ops->write_inode(dentry->inode);
    return 0;
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
    if (inode == NULL) return -EINVAL;
    if (inode->fs_specific == NULL) return -EINVAL;

    ext2_inode_data_t* data = (ext2_inode_data_t*)inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    if (data->inode_number == 0) return -EINVAL;

    ext2_block_group_t* bg = &vol->bgdt[data->block_group];

    mutex_lock(&vol->inode_alloc_lock);

    uint64_t* bitmap = (uint64_t*) bread(inode->sb, bg->inode_bitmap);

    uint32_t inode_idx_inside_bitmap = (data->inode_number - 1) - vol->inodes_per_group * data->block_group;
    uint64_t u64_idx = inode_idx_inside_bitmap / 64, bit_idx = inode_idx_inside_bitmap % 64;

    bitmap[u64_idx] &= ~(1ULL << bit_idx);
    bwrite(inode->sb, bg->inode_bitmap);
    brelse(inode->sb, bg->inode_bitmap);

    if (inode->type == VFS_TYPE_DIR) bg->used_dirs_count--;
    bg->free_inodes_count++;
    vol->free_inodes++;

    mutex_unlock(&vol->inode_alloc_lock);
    return 0;
}

int64_t EXT2DeleteBlockFromBG(inode_t* inode, uint64_t block_idx) {
    if (inode == NULL || block_idx == 0) return -EINVAL;
    if (inode->fs_specific == NULL) return -EINVAL;

    ext2_inode_data_t* data = (ext2_inode_data_t*)inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    if (data->inode_number == 0) return -EINVAL;

    uint32_t bg_idx = block_idx / vol->blocks_per_group;
    ext2_block_group_t* bg = &vol->bgdt[bg_idx];
    bg->free_blocks_count++;
    vol->free_blocks++;
    
    uint64_t* bitmap = (uint64_t*) bread(inode->sb, bg->block_bitmap);

    uint32_t block_idx_inside_bitmap = (block_idx - vol->first_data_block - 1) - vol->blocks_per_group * bg_idx;
    uint64_t u64_idx = block_idx_inside_bitmap / 64, bit_idx = block_idx_inside_bitmap % 64;

    bitmap[u64_idx] &= ~(1ULL << bit_idx);
    bwrite(inode->sb, bg->block_bitmap);
    brelse(inode->sb, bg->block_bitmap);

    return 0;
}

int64_t EXT2DeleteDataBlocksFromBG(inode_t* inode) {
    if (inode == NULL || inode->fs_specific == NULL) return -EINVAL;
    if (((ext2_inode_data_t*)inode->fs_specific)->inode_number == 0) return -EINVAL;

    return EXT2FreeBlocksFrom(inode, 0);
}

int64_t SetDeleteTime(inode_t* inode) {
    total_time_t t;
    GetTotalTime(&t);

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    data->i_dtime = CalculateUnixTimestamp(&t);
    return 0;
}

int64_t EXT2RemoveFromDir(inode_t* dir, dentry_t* dentry) {
    if (dir == NULL || dentry == NULL) return -EINVAL;
    if (dir->fs_specific == NULL || dentry->name == NULL) return -EINVAL;

    uint32_t dir_blocks = dir->size / dir->sb->block_size;

    for (uint32_t i = 0; i < dir_blocks; i++) {
        int64_t block_number = FindDataBlock(dir, i);
        if (block_number == 0) continue;

        ext2_dir_entry_t* entries = (ext2_dir_entry_t*) bread(dir->sb, block_number), *p1, *p2;
        p1 = (ext2_dir_entry_t*)((uint64_t)entries);
        
        if (strncmp(p1->name, dentry->name, p1->name_len) == 0) {
            p1->inode = 0;
            brelse(dir->sb, block_number);
            return 0;
        }

        uint32_t offset = 0;

        while (offset + p1->rec_len < dir->sb->block_size) {
            
            p2 = (ext2_dir_entry_t*)((uint64_t)p1 + p1->rec_len);

            if (p2->inode != 0 && strncmp(p2->name, dentry->name, p2->name_len) == 0) {
                p1->rec_len += p2->rec_len;

                bwrite(dir->sb, block_number);
                brelse(dir->sb, block_number);
                return 0;
            }
            offset += p1->rec_len;
            p1 = (ext2_dir_entry_t*)((uint64_t)entries + offset);

        }
        brelse(dir->sb, block_number);
    }
    return -ENOENT;
}

int64_t EXT2Unlink(inode_t* dir, dentry_t* dentry) {
    int64_t lookup_ret = EXT2Lookup(dir, dentry);
    if (lookup_ret < 0) return lookup_ret;
    if (dentry->inode->type == VFS_TYPE_DIR) return -EISDIR;

    EXT2RemoveFromDir(dir, dentry);

    ext2_inode_data_t* del_data = (ext2_inode_data_t*) dentry->inode->fs_specific;
    inode_t* del = dentry->inode;
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

int64_t EXT2Rmdir(inode_t* dir, dentry_t* dentry) {
    int64_t lookup_ret = EXT2Lookup(dir, dentry);
    if (lookup_ret < 0) return lookup_ret;

    if (dentry->inode->type != VFS_TYPE_DIR) return -ENOTDIR;
    if (((ext2_inode_data_t*)dentry->inode->fs_specific)->inode_number == EXT2_ROOT_INO) return -EACCES;

    uint32_t last_entry_block, last_entry_byte;
    int64_t last_entry_ret = EXT2FindLastDirEntryLocation(dentry->inode, &last_entry_block, &last_entry_byte);

    if (last_entry_ret < 0) return last_entry_ret;

    ext2_dir_entry_t* last_entry = (ext2_dir_entry_t*) ((uint64_t)bread(dir->sb, last_entry_block) + last_entry_byte);

    if (last_entry->inode == 0) return -1;
    if (last_entry->name_len != 2) return -ENOTEMPTY;
    if (strncmp(last_entry->name, "..", 2) != 0) return -ENOTEMPTY;

    EXT2RemoveFromDir(dir, dentry);

    ext2_inode_data_t* del_data = (ext2_inode_data_t*) dentry->inode->fs_specific;
    inode_t* del = dentry->inode;
    del_data->ref_count--;

    if (del_data->ref_count == 0) {
        EXT2DeleteInodeFromBG(del);
        EXT2DeleteDataBlocksFromBG(del);
        SetDeleteTime(del);
        del->sb->ops->write_inode(del);
        del->sb->ops->free_inode(del);
    }

    ext2_inode_data_t* dir_data = (ext2_inode_data_t*)dir->fs_specific;
    dir_data->ref_count--;
    dir->sb->ops->write_inode(dir);
    return 0;
}

int64_t EXT2Rename(inode_t* old_dir, dentry_t* old_dentry, inode_t* new_dir, dentry_t* new_dentry) {
    if (old_dir == NULL || old_dentry == NULL || new_dir == NULL || new_dentry == NULL) return -EINVAL;
    if (old_dentry->inode == NULL) return -EINVAL;

    int64_t remv_ret = EXT2RemoveFromDir(old_dir, old_dentry);
    if (remv_ret < 0) return remv_ret;
    new_dentry->inode = old_dentry->inode;
    
    int64_t pop_ret = EXT2PopulateDirEntry(new_dir, new_dentry, new_dentry->inode->type);
    if (pop_ret < 0) return pop_ret;
    return 0;
}

int64_t EXT2HardLink(inode_t* dir, inode_t* existing_inode, dentry_t* dentry) {
    if (dir == NULL || existing_inode == NULL || dentry == NULL) return -EINVAL;
    
    dentry->inode = existing_inode;
    EXT2PopulateDirEntry(dir, dentry, dentry->inode->type);

    ext2_inode_data_t* data = (ext2_inode_data_t*)dentry->inode->fs_specific;
    data->ref_count++;
    dentry->inode->sb->ops->write_inode(dentry->inode);
    return 0;
}

int64_t EXT2SymLink(inode_t* dir, dentry_t* dentry, const char* target) {
    if (dir == NULL || dentry == NULL || target == NULL) return -EINVAL;

    uint64_t name_len = strlen(target);
    if (name_len > dir->sb->block_size) return -EINVAL;

    int64_t create_ret = EXT2CreateGeneric(dir, dentry, 0777, VFS_TYPE_SYMLINK);
    if (create_ret < 0) return create_ret;

    ext2_inode_data_t* data = (ext2_inode_data_t*)dentry->inode->fs_specific;

    
    if (name_len <= MAX_FAST_SYMLINK_LENGTH) {
        memcpy(data->i_block, target, name_len);
        dentry->inode->size = name_len;
        dentry->inode->sb->ops->write_inode(dentry->inode);
        return 0;
    }

    uint32_t new_block = EXT2AllocBlock(dentry->inode->sb, data->block_group);
    if (new_block == 0) return -ENOSPC;
    int64_t addblock_ret = EXT2AddBlockToInode(dentry->inode, new_block);
    if (addblock_ret < 0) return addblock_ret;

    void* buf = (void*)bread(dentry->inode->sb, new_block);
    memcpy(buf, target, name_len);
    bwrite(dentry->inode->sb, new_block);

    dentry->inode->size = name_len;
    data->i_blocks = dentry->inode->sb->block_size / dentry->inode->sb->bdev->sector_size;
    dentry->inode->sb->ops->write_inode(dentry->inode);
    return 0;
}

int64_t EXT2ReadLink(inode_t* inode, char* buf, uint64_t size) {
    if (inode == NULL || buf == NULL || size == 0) return -EINVAL;
    uint64_t cpy_size = (inode->size > size) ? size : inode->size;
    ext2_inode_data_t* data = inode->fs_specific;

    if (inode->size <= MAX_FAST_SYMLINK_LENGTH) {
        memcpy(buf, data->i_block, cpy_size);
        return cpy_size;
    }

    uint64_t num_blocks = (cpy_size + inode->sb->block_size - 1) / inode->sb->block_size;
    uint64_t remain_size = cpy_size, specific_cpy;
    uint32_t data_block;

    for (uint64_t i = 0; i < num_blocks; i++) {
        data_block = FindDataBlock(inode, i);
        if (data_block == 0) continue;

        void* src = bread(inode->sb, data_block);
        specific_cpy = (remain_size > inode->sb->block_size) ? inode->sb->block_size : remain_size;

        memcpy(buf, src, specific_cpy);
        remain_size -= specific_cpy;
    }
    return cpy_size;
}

int64_t EXT2GetAttr(inode_t* inode, fs_inode_stat_t* out) {
    if (inode == NULL || out == NULL) return -EINVAL;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    out->type      = inode->type;
    out->mode      = inode->permissions & 0x0FFF;
    out->size      = inode->size;
    out->uid       = inode->owner_id;
    out->gid       = inode->group_id;
    out->dev_id    = inode->dev_id;
    out->reserved_ = 0;

    if (data != NULL) {
        out->nlinks = data->ref_count;          // EXT2 mirrors i_links_count here
        out->atime  = data->accessed_at;
        out->mtime  = data->modified_at;
        out->ctime  = data->changed_at;
    } else {
        out->nlinks = 0;
        out->atime  = 0;
        out->mtime  = 0;
        out->ctime  = 0;
    }
    return 0;
}

inode_ops_t ext2_inode_ops = {
    .lookup   = EXT2Lookup,
    .create   = EXT2Create,
    .mkdir    = EXT2Mkdir,
    .rmdir    = EXT2Rmdir,
    .unlink   = EXT2Unlink,
    .rename   = EXT2Rename,
    .hardlink = EXT2HardLink,
    .mknod    = EXT2Mknod,
    .symlink  = EXT2SymLink,
    .readlink = EXT2ReadLink,
    .getattr  = EXT2GetAttr,
    .setattr  = NULL,
};