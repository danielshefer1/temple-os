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
        part->physical_device->read(part->physical_device, part->start_lba + blocks_offset, sectors_count,(void*)KERNEL_VIRT_TO_PHYS(buf));

        sector_offset = EXT2_SUPERBLOCK_OFFSET % sector_size;
        ext2_superblock_t* sbext = (ext2_superblock_t*)(buf + sector_offset);

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

int64_t EXT2Mount(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->bdev == NULL) return 2;

    uint64_t sector_size = sb->bdev->sector_size;

    uint64_t blocks_offset = EXT2_SUPERBLOCK_OFFSET / sector_size;
    uint64_t sectors_count = (EXT2_SUPERBLOCK_LENGTH + sector_size  - 1) / sector_size;
    uint64_t pages_count = (EXT2_SUPERBLOCK_LENGTH + PAGE_SIZE - 1) / (PAGE_SIZE);
    uint64_t buf = AddKernelPages(pages_count);
    sb->bdev->read(sb->bdev, sb->start_lba + blocks_offset, sectors_count,(void*)KERNEL_VIRT_TO_PHYS(buf));

    uint64_t sector_offset = EXT2_SUPERBLOCK_OFFSET % sector_size;
    ext2_superblock_t* sbext = (ext2_superblock_t*)(buf + sector_offset);
    if (sbext->s_magic != EXT2_MAGIC) return 3;

    sb->fs_info = kmalloc(sizeof(ext2_internal_info_t));
    CopySbExtToInternal(sbext, (ext2_internal_info_t*)sb->fs_info);

    ext2_internal_info_t* vol = (ext2_internal_info_t*) sb->fs_info;
    sb->block_size = vol->block_size;

    RemoveKernelPages(buf, pages_count);
    return 0;
}

int64_t CopySbExtToInternal(ext2_superblock_t* sbext, ext2_internal_info_t* vol) {
    if (sbext->s_magic != EXT2_MAGIC) {
        kprintf("ext2: invalid magic: %x\n", sbext->s_magic);
        return -1;
    }

    vol->block_size = EXT2_BLOCK_SIZE(sbext);
    vol->inodes_per_group = sbext->s_inodes_per_group;
    vol->blocks_per_group = sbext->s_blocks_per_group;
    vol->inode_size = sbext->s_rev_level >= 1 ? sbext->s_inode_size : 128;
    vol->first_data_block = sbext->s_first_data_block;
    vol->first_ino = sbext->s_rev_level >= 1 ? sbext->s_first_ino : 11;
    vol->total_inodes = sbext->s_inodes_count;
    vol->total_blocks = sbext->s_blocks_count;
    vol->free_inodes = sbext->s_free_inodes_count;
    vol->free_blocks = sbext->s_free_blocks_count;

    uint32_t sb_block = sbext->s_first_data_block;
    vol->gdt_lba = sb_block + 1;

    return 0;
}