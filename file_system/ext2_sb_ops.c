#include "ext2_sb_ops.h"

static superblock_ops_t ext2_sb_ops = {
    .alloc_inode = EXT2AllocInode,
    .free_inode = EXT2FreeInode,
    .read_inode = EXT2ReadInode,
    .write_inode = EXT2WriteInode,
    .mount = EXT2Mount,
    .unmount = EXT2Umount,
    .sync = EXT2Sync,
    .stat = NULL
};


superblock_t* EXT2MountRoot() {
    superblock_t* sb = (superblock_t*) kmalloc(sizeof(superblock_t));
    int64_t ret = EXT2FindRoot(sb);

    if (ret == 0) return sb;

    kfree(sb, sizeof(superblock_t));
    return NULL;
}

int64_t EXT2FindRoot(superblock_t* sb) {
    if (parts_head == NULL) return -1;
    partition_device_node_t* p = parts_head;
    partition_device_t* part;
    uint64_t sector_size, blocks_offset, sectors_count, pages_count, buf, sector_offset;

    while (p != NULL) {
        part = p->value;

        sector_size = part->physical_device->sector_size;

        blocks_offset = EXT2_SUPERBLOCK_OFFSET / sector_size;
        sectors_count = (EXT2_SUPERBLOCK_LENGTH + sector_size  - 1) / sector_size;
        pages_count = (EXT2_SUPERBLOCK_LENGTH + PAGE_SIZE - 1) / (PAGE_SIZE);
        buf = AddKernelPages(pages_count);
        part->physical_device->read(part->physical_device, part->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

        sector_offset = EXT2_SUPERBLOCK_OFFSET % sector_size;
        ext2_superblock_disk_t* sbext = (ext2_superblock_disk_t*)(buf + sector_offset);

        if (sbext->s_magic == EXT2_MAGIC) {
            if (strcmp(sbext->s_volume_name, ROOT_LABEL) == 0) {
                sb->bdev = part->physical_device;
                sb->start_lba = part->start_lba;
                int64_t ret = EXT2Mount(sb);

                RemoveKernelPages(buf, pages_count);
                if (ret != 0) return ret;

                return 0;
            }
        }
        RemoveKernelPages(buf, pages_count);
        p = p->next;
    }
    if (buf != 0 && pages_count != 0) RemoveKernelPages(buf, pages_count);

    return 1;
}

int64_t EXT2ReadBGDT(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->fs_info == NULL) return 1;
    if (sb->block_size == 0) return 1;

    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    uint32_t bgdt_block = (sb->block_size == 1024) ? 2 : 1;

    uint32_t raw_size = vol->block_group_count * sizeof(ext2_block_group_desc_disk_t);
    uint32_t raw_size_pages = (raw_size + PAGE_SIZE - 1) / PAGE_SIZE;
    ext2_block_group_desc_disk_t* buf = bread(sb, bgdt_block);

    vol->bgdt = (ext2_block_group_t*) kmalloc(vol->block_group_count * sizeof(ext2_block_group_t));

    for (uint32_t i = 0; i < vol->block_group_count; i++) {
        vol->bgdt[i].block_bitmap      = buf[i].block_bitmap;
        vol->bgdt[i].inode_bitmap      = buf[i].inode_bitmap;
        vol->bgdt[i].inode_table       = buf[i].inode_table;
        vol->bgdt[i].free_blocks_count = buf[i].free_blocks_count;
        vol->bgdt[i].free_inodes_count = buf[i].free_inodes_count;
        vol->bgdt[i].used_dirs_count   = buf[i].used_dirs_count;
    }
 
    brelse(sb, bgdt_block);
    return 0;
}

int64_t EXT2WriteBGDT(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->fs_info == NULL) return 1;
    if (sb->block_size == 0) return 1;

    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    uint32_t bgdt_block = (sb->block_size == 1024) ? 2 : 1;

    uint32_t raw_size = vol->block_group_count * sizeof(ext2_block_group_desc_disk_t);
    uint32_t raw_size_pages = (raw_size + PAGE_SIZE - 1) / PAGE_SIZE;
    ext2_block_group_desc_disk_t* buf = bread(sb, bgdt_block);

    for (uint32_t i = 0; i < vol->block_group_count; i++) {
        buf[i].block_bitmap = vol->bgdt[i].block_bitmap;
        buf[i].inode_bitmap = vol->bgdt[i].inode_bitmap;
        buf[i].inode_table = vol->bgdt[i].inode_table;
        buf[i].free_blocks_count = vol->bgdt[i].free_blocks_count;
        buf[i].free_inodes_count = vol->bgdt[i].free_inodes_count;
        buf[i].used_dirs_count = vol->bgdt[i].used_dirs_count;
    }

    bwrite(sb, bgdt_block);
 
    return 0;
}

int64_t CopySbExtToInternal(ext2_superblock_disk_t* sbext, superblock_t* sb) {
    if (sbext->s_magic != EXT2_MAGIC) {
        kprintf("ext2: invalid magic: %x\n", sbext->s_magic);
        return -1;
    }

    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    sb->block_size = EXT2_BLOCK_SIZE(sbext);
    sb->pages_in_block = (sb->block_size + PAGE_SIZE - 1) / PAGE_SIZE;

    vol->sectors_per_block = sb->block_size / sb->bdev->sector_size;

    vol->inodes_per_group = sbext->s_inodes_per_group;
    vol->blocks_per_group = sbext->s_blocks_per_group;

    vol->first_data_block = sbext->s_first_data_block;
    vol->first_usable_inode = sbext->s_rev_level >= 1 ? sbext->s_first_ino : 11;
    vol->root_inode_number = EXT2_ROOT_INO;

    vol->total_inodes = sbext->s_inodes_count;
    vol->total_blocks = sbext->s_blocks_count;

    vol->inode_size = sbext->s_rev_level >= 1 ? sbext->s_inode_size : 128;
    vol->block_group_count = vol->total_blocks / vol->blocks_per_group;

    vol->free_inodes = sbext->s_free_inodes_count;
    vol->free_blocks = sbext->s_free_blocks_count;

    vol->feature_compat = sbext->s_feature_compat;
    vol->feature_incompat = sbext->s_feature_incompat;
    vol->feature_ro_compat = sbext->s_feature_ro_compat;

    memcpy(vol->hash_seed, sbext->s_hash_seed, sizeof(vol->hash_seed));
    vol->def_hash_version = sbext->s_def_hash_version;

    int64_t ret = EXT2ReadBGDT(sb);

    if (ret != 0) return ret + 1;

    return 0;
}

int64_t CopyInternalToSbExt(superblock_t* sb, ext2_superblock_disk_t* sbext) {
    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    sbext->s_free_inodes_count = vol->free_inodes;
    sbext->s_free_blocks_count = vol->free_blocks;

    EXT2WriteBGDT(sb);
    return 0;
}

int64_t EXT2Mount(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->bdev == NULL) return 2;

    uint64_t sector_size = sb->bdev->sector_size;

    uint64_t blocks_offset = EXT2_SUPERBLOCK_OFFSET / sector_size;
    uint64_t sectors_count = (EXT2_SUPERBLOCK_LENGTH + sector_size  - 1) / sector_size;
    uint64_t pages_count = (EXT2_SUPERBLOCK_LENGTH + PAGE_SIZE - 1) / (PAGE_SIZE);
    uint64_t buf = AddKernelPages(pages_count);
    sb->bdev->read(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    uint64_t sector_offset = EXT2_SUPERBLOCK_OFFSET % sector_size;
    ext2_superblock_disk_t* sbext = (ext2_superblock_disk_t*)(buf + sector_offset);
    if (sbext->s_magic != EXT2_MAGIC) return 3;

    sb->fs_info = kmalloc(sizeof(ext2_info_t));
    int64_t ret = CopySbExtToInternal(sbext, sb);

    total_time_t total_time;
    GetTotalTime(&total_time);
    uint32_t mtime = CalculateUnixTimestamp(&total_time);
    if (mtime != 0 && ret == 0) sbext->s_mtime = mtime;
    sbext->s_state = EXT2_ERROR_FS;
    sbext->s_mnt_count++;

    sb->bdev->write(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    RemoveKernelPages(buf, pages_count);

    inode_t* root_inode = EXT2AllocInode(sb);
    ext2_inode_data_t* root_data = (ext2_inode_data_t*) root_inode->fs_specific;
    root_data->inode_number = EXT2_ROOT_INO;

    EXT2ReadInode(root_inode);
    sb->root_inode = root_inode;
    sb->ops = &ext2_sb_ops;

    if (ret != 0) return ret + 1;
    return 0;
}

int64_t EXT2Sync(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->bdev == NULL) return 1;

    bflush(sb);
    // Complete later when we have more info about inodes and file ops, for now just flush the block cache and update the superblock state //


    return 0;
}

int64_t EXT2Umount(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->bdev == NULL) return 1;

    EXT2Sync(sb);

    uint64_t sector_size = sb->bdev->sector_size;

    uint64_t blocks_offset = EXT2_SUPERBLOCK_OFFSET / sector_size;
    uint64_t sectors_count = (EXT2_SUPERBLOCK_LENGTH + sector_size  - 1) / sector_size;
    uint64_t pages_count = (EXT2_SUPERBLOCK_LENGTH + PAGE_SIZE - 1) / (PAGE_SIZE);
    uint64_t buf = AddKernelPages(pages_count);
    sb->bdev->read(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    uint64_t sector_offset = EXT2_SUPERBLOCK_OFFSET % sector_size;
    ext2_superblock_disk_t* sbext = (ext2_superblock_disk_t*)(buf + sector_offset);
    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    sbext->s_state = EXT2_VALID_FS;
    CopyInternalToSbExt(sb, sbext);
    bflush(sb);

    sb->bdev->write(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    RemoveKernelPages(buf, pages_count);
    return 0; // IMPORTANT - Fill the rest when I know how to deal with inode, now just clean the sb
}



inode_t* EXT2AllocInode(superblock_t* sb) {
    if (sb == NULL) return NULL;

    inode_t* inode = (inode_t*) kmalloc(sizeof(inode_t));
    if (inode == NULL || (uint64_t)inode == KERNEL_VIRTUAL) return NULL;

    inode->fs_specific = kmalloc(sizeof(ext2_inode_data_t));
    if (inode->fs_specific == NULL || (uint64_t)inode->fs_specific == KERNEL_VIRTUAL) {
        kfree(inode, sizeof(inode_t));
        return NULL;
    };

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    inode->sb = sb;
    inode->ops = NULL; // IMPORTANT - fill in with ext2 ops later - IMPORTANT

    data->block_group = UINT32_MAX;

    return inode;
}

int64_t EXT2FreeInode(inode_t* inode) {
    // IMPORTANT - Fill later when we have more inode and file ops! //
    kfree(inode->fs_specific, sizeof(ext2_inode_data_t));
    kfree(inode, sizeof(inode_t));
    return 0;
}

void PopulateInode(ext2_inode_disk_t* raw, inode_t* inode) {
    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    inode->size        = raw->i_size;
    inode->type        = EXT2ModeToType(raw->i_mode);

    inode->permissions = raw->i_mode & 0x0FFF;
    inode->owner_id    = raw->i_uid;
    inode->group_id    = raw->i_gid;
    inode->flags      = EXT2FlagsToVFSFlags(raw->i_flags);

    data->ref_count    = raw->i_links_count;
    data->changed_at   = raw->i_ctime;
    data->modified_at  = raw->i_mtime;
    data->accessed_at  = raw->i_atime;

    data->i_blocks     = raw->i_blocks;
    data->i_generation = raw->i_generation;
    data->i_file_acl   = raw->i_file_acl;
    data->i_dir_acl    = raw->i_dir_acl;
    data->i_faddr      = raw->i_faddr;
    data->i_osd1       = raw->i_osd1;
    data->i_dtime      = raw->i_dtime;
    memcpy(data->i_osd2,  raw->i_osd2,  sizeof(raw->i_osd2));
    memcpy(data->i_block, raw->i_block, sizeof(raw->i_block));

    if (inode->type == VFS_TYPE_FILE)
        inode->size |= ((uint64_t)raw->i_dir_acl << 32);
}

/*
    Fields in inode that have to be filled before you call this func:
    1. inode number
    2. superblock
    3. block group (Optional)
    Every other field is filled here.
*/
int64_t EXT2ReadInode(inode_t* inode) {
    if (inode == NULL) return 1;
    if (inode->sb == NULL) return 1;
    if (inode->fs_specific == NULL) return 1;


    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    superblock_t* sb = inode->sb;

    if (data->block_group == UINT32_MAX) data->block_group = EXT2InodeNumberToGroup(vol, data->inode_number);

    if (data->disk_offset == 0) {
        uint32_t inode_index    = (data->inode_number - 1) % vol->inodes_per_group;
        uint32_t inode_table    = vol->bgdt[data->block_group].inode_table;
        uint32_t block_offset   = (inode_index * vol->inode_size) / sb->block_size;
        uint32_t byte_offset    = (inode_index * vol->inode_size) % sb->block_size;

        data->disk_offset = (inode_table + block_offset) * sb->block_size + byte_offset;
    }
    uint32_t block_offset, bytes_offset;

    block_offset = data->disk_offset / sb->block_size;
    bytes_offset = data->disk_offset % sb->block_size;

    

    uint8_t* buf = (uint8_t*) bread(sb, block_offset);
    ext2_inode_disk_t* raw_inode = (ext2_inode_disk_t*) (buf + bytes_offset);

    PopulateInode(raw_inode, inode);
    total_time_t total_time;
    GetTotalTime(&total_time);
    uint32_t unix_timestamp = CalculateUnixTimestamp(&total_time);

    if (unix_timestamp != 0) {
        data->accessed_at    = unix_timestamp;
    }
    brelse(sb, block_offset);

    return 0;
}

void PopulateRawInode(inode_t* inode, ext2_inode_disk_t* raw) {
    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;

    raw->i_size        = inode->size;
    raw->i_mode        = EXT2TypeToMode(inode->type) | (inode->permissions & 0x0FFF);
    raw->i_uid         = inode->owner_id;
    raw->i_gid         = inode->group_id;
    raw->i_flags       = EXT2VFSFlagsToIFlags(inode->flags);

    raw->i_links_count = data->ref_count;
    raw->i_ctime       = data->changed_at;
    raw->i_mtime       = data->modified_at;
    raw->i_atime       = data->accessed_at;

    raw->i_blocks      = data->i_blocks;
    
    raw->i_generation  = data->i_generation;
    raw->i_file_acl    = data->i_file_acl;
    raw->i_dir_acl     = data->i_dir_acl;
    raw->i_faddr       = data->i_faddr;
    raw->i_osd1        = data->i_osd1;
    raw->i_dtime       = data->i_dtime;
    memcpy(raw->i_osd2,  data->i_osd2,  sizeof(raw->i_osd2));
    memcpy(raw->i_block, data->i_block, sizeof(raw->i_block));
}

int64_t EXT2WriteInode(inode_t* inode) {
    if (inode == NULL) return 1;
    if (inode->sb == NULL) return 1;
    if (inode->fs_specific == NULL) return 1;


    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    ext2_info_t* vol = (ext2_info_t*) inode->sb->fs_info;
    superblock_t* sb = inode->sb;

    total_time_t total_time;
    GetTotalTime(&total_time);
    data->changed_at = CalculateUnixTimestamp(&total_time);

    if (data->disk_offset == 0) {
        uint32_t inode_index    = (data->inode_number - 1) % vol->inodes_per_group;
        uint32_t inode_table    = vol->bgdt[data->block_group].inode_table;
        uint32_t block_offset   = (inode_index * vol->inode_size) / sb->block_size;
        uint32_t byte_offset    = (inode_index * vol->inode_size) % sb->block_size;

        data->disk_offset = (inode_table + block_offset) * sb->block_size + byte_offset;
    }

    uint32_t block_offset   = data->disk_offset / sb->block_size;
    uint32_t byte_offset    = data->disk_offset % sb->block_size;

    uint8_t* buf = (uint8_t*) bread(sb, block_offset);

    ext2_inode_disk_t* raw_inode = (ext2_inode_disk_t*) (buf + byte_offset);
    PopulateRawInode(inode, raw_inode);
    bwrite(sb, block_offset);
    brelse(sb, block_offset);

    return 0;
}

