#include "ext2_sb_ops.h"

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
    ext2_block_group_desc_disk_t* buf = (ext2_block_group_desc_disk_t*) AddKernelPages(raw_size_pages);
    EXT2ReadBlocks(sb, bgdt_block, (raw_size + sb->block_size - 1) / sb->block_size, (void*)buf);

    vol->bgdt = (ext2_block_group_t*) kmalloc(vol->block_group_count * sizeof(ext2_block_group_t));

    for (uint32_t i = 0; i < vol->block_group_count; i++) {
        vol->bgdt[i].block_bitmap      = buf[i].block_bitmap;
        vol->bgdt[i].inode_bitmap      = buf[i].inode_bitmap;
        vol->bgdt[i].inode_table       = buf[i].inode_table;
        vol->bgdt[i].free_blocks_count = buf[i].free_blocks_count;
        vol->bgdt[i].free_inodes_count = buf[i].free_inodes_count;
        vol->bgdt[i].used_dirs_count   = buf[i].used_dirs_count;
    }

    RemoveKernelPages((uint64_t)buf, raw_size_pages);
 
    return 0;
}

int64_t CopySbExtToInternal(ext2_superblock_disk_t* sbext, superblock_t* sb) {
    if (sbext->s_magic != EXT2_MAGIC) {
        kprintf("ext2: invalid magic: %x\n", sbext->s_magic);
        return -1;
    }

    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    sb->block_size = EXT2_BLOCK_SIZE(sbext);
    vol->sectors_per_block = sb->block_size / sb->bdev->sector_size;

    vol->inodes_per_group = sbext->s_inodes_per_group;
    vol->blocks_per_group = sbext->s_blocks_per_group;

    vol->first_data_block = sbext->s_first_data_block;
    vol->first_usable_inode = sbext->s_rev_level >= 1 ? sbext->s_first_ino : 11;
    vol->root_inode_number = ROOT_INODE;

    vol->total_inodes = sbext->s_inodes_count;
    vol->total_blocks = sbext->s_blocks_count;

    vol->inode_size = sbext->s_rev_level >= 1 ? sbext->s_inode_size : 128;
    vol->block_group_count = vol->total_blocks / vol->blocks_per_group;

    vol->free_inodes = sbext->s_free_inodes_count;
    vol->free_blocks = sbext->s_free_blocks_count;

    vol->feature_compat = sbext->s_feature_compat;
    vol->feature_incompat = sbext->s_feature_incompat;
    vol->feature_ro_compat = sbext->s_feature_ro_compat;

    int64_t ret = EXT2ReadBGDT(sb);

    if (ret != 0) return ret + 1;

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
    //sbext->s_state = 0x02;

    sb->bdev->write(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    RemoveKernelPages(buf, pages_count);

    if (ret != 0) return ret + 1;
    return 0;
}

int64_t EXT2Sync(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->bdev == NULL) return 1;

    uint64_t sector_size = sb->bdev->sector_size;

    uint64_t blocks_offset = EXT2_SUPERBLOCK_OFFSET / sector_size;
    uint64_t sectors_count = (EXT2_SUPERBLOCK_LENGTH + sector_size  - 1) / sector_size;
    uint64_t pages_count = (EXT2_SUPERBLOCK_LENGTH + PAGE_SIZE - 1) / (PAGE_SIZE);
    uint64_t buf = AddKernelPages(pages_count);
    sb->bdev->read(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    uint64_t sector_offset = EXT2_SUPERBLOCK_OFFSET % sector_size;
    ext2_superblock_disk_t* sbext = (ext2_superblock_disk_t*)(buf + sector_offset);
    ext2_info_t* vol = (ext2_info_t*) sb->fs_info;

    sbext->s_free_blocks_count = vol->free_blocks;
    sbext->s_free_inodes_count = vol->free_inodes;

    total_time_t total_time;
    GetTotalTime(&total_time);
    uint32_t wtime = CalculateUnixTimestamp(&total_time);
    if (wtime != 0) sbext->s_wtime = wtime;

    sb->bdev->write(sb->bdev, sb->start_lba + blocks_offset, sectors_count, (void*)KERNEL_VIRT_TO_PHYS(buf));

    return 0;
}

int64_t EXT2Umount() {

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
}

void EXT2FreeInode(superblock_t* sb, inode_t* inode) {
}

uint32_t EXT2SectorsInBlock(superblock_t* sb) {
    return sb->block_size / sb->bdev->sector_size;
}

uint64_t EXT2BlockToLba(superblock_t* sb, uint32_t block_idx) {
    uint32_t sectors_per_block = sb->block_size / sb->bdev->sector_size;

    return sb->start_lba + (block_idx * sectors_per_block);
}

int64_t EXT2ReadBlocks(superblock_t* sb, uint32_t block_idx, uint32_t count, void* buf) {
    if (sb == NULL || buf == NULL || count == 0 || block_idx == 0) return 1;

    uint64_t sectors_count = count * EXT2SectorsInBlock(sb);
    sb->bdev->read(sb->bdev, EXT2BlockToLba(sb, block_idx), count * EXT2SectorsInBlock(sb), (void*)KERNEL_VIRT_TO_PHYS(buf));
    return 0;

}

