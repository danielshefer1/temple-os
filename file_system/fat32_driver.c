#include "fat32_driver.h"


bool fat32_is_eoc(uint32_t val) {return val >= 0x0FFFFFF8;}
bool fat32_is_free(uint32_t val) {return val == 0x00000000;}
bool fat32_is_bad(uint32_t val) {return val == 0x0FFFFFF7;}
bool fat32_is_valid(uint32_t val) {return val >= 0x00000002 && val <= 0x0FFFFFEF;}

void CopyBPB_IntoInfo(fat32_internal_info_t* vol, fat32_bpb_t* bpb, superblock_t* sb) {
    vol->bdev = sb->bdev;
    vol->bytes_per_sector = bpb->bytes_per_sector;
    vol->sectors_per_cluster = bpb->sectors_per_cluster;
    vol->reserved_sectors = bpb->reserved_sectors;
    vol->number_of_fats = bpb->fat_count;
    vol->fat_size = bpb->fat_size_32;
    vol->root_cluster = bpb->root_cluster;

    vol->fat_start_lba = sb->start_lba + bpb->reserved_sectors;

    vol->data_start_lba = sb->start_lba + bpb->reserved_sectors
                           + (bpb->fat_count * bpb->fat_size_32);

    vol->total_clusters = (bpb->total_sectors_32
                                 - bpb->reserved_sectors
                                 - (bpb->fat_count * bpb->fat_size_32))
                                 / bpb->sectors_per_cluster;

    vol->free_clusters      = 0;
    vol->last_alloc_cluster = 0;
}

superblock_t* Fat32MountRootWrapper() {
    superblock_t* sb = (superblock_t*) kmalloc(sizeof(superblock_t));
    int64_t ret_code = Fat32MountRoot(sb);
    if (ret_code != 0) {
        kfree(sb, sizeof(sb));
        return NULL;
    }
    return sb;
}

int64_t Fat32MountRoot(superblock_t* sb) {
    if (parts_head == NULL) return -1;
    if (sb == NULL) return 1;
    fat32_bpb_t* buffer = (fat32_bpb_t*)AddNonCachableKernelPages(1);

    partition_device_node_t* p = parts_head;
    partition_device_t* part;

    while (p != NULL) {
        part = p->value;
        part->physical_device->read(part->physical_device, part->start_lba, SECTOR_SIZE, (void*) KERNEL_VIRT_TO_PHYS(buffer));

        if (strncmp(buffer->volume_label, ROOT_LABEL, ROOT_LABEL_LENGTH) == 0) {
            sb->bdev = part->physical_device;
            sb->start_lba = part->start_lba;
            int64_t ret_code = Fat32Mount(sb);
            if (ret_code != 0) {
                kfree(sb->fs_info, sizeof(fat32_internal_info_t));
                return 2;
            }
            return 0;
        }
        p = p->next;
    }
    RemoveKernelPages((uint64_t)buffer, 1); 
    return 1;
}

int64_t Fat32Mount(superblock_t* sb) {
    if (sb == NULL) return 1;
    if (sb->bdev == NULL) return 2;

    fat32_bpb_t* buffer = (fat32_bpb_t*)AddNonCachableKernelPages(1);

    sb->bdev->read(sb->bdev, sb->start_lba, SECTOR_SIZE, (void*)KERNEL_VIRT_TO_PHYS(buffer));
    sb->fs_info = kmalloc(sizeof(fat32_internal_info_t));

    CopyBPB_IntoInfo((fat32_internal_info_t*)sb->fs_info, buffer, sb);
    sb->magic = FAT32_MAGIC;
    fat32_internal_info_t* vol = (fat32_internal_info_t*)sb->fs_info;
    sb->block_size = vol->bytes_per_sector * vol->sectors_per_cluster;

    RemoveKernelPages((uint64_t)buffer, 1);
    return 0;
}

uint32_t ClusterToLBA(uint32_t cluster, fat32_internal_info_t* vol) {
    return vol->data_start_lba + (cluster - 2)*vol->sectors_per_cluster;
}

uint32_t FatNextCluster(uint32_t cluster, fat32_internal_info_t* vol, void* buf) {
    uint32_t fat_offset   = cluster * 4;
    uint32_t fat_sector   = vol->fat_start_lba + (fat_offset / vol->bytes_per_sector);
    uint32_t entry_offset = fat_offset % vol->bytes_per_sector;
    bool alloc = false;

    if (buf == NULL) {buf = (void*)AddNonCachableKernelPages(1); alloc = true;}

    vol->bdev->read(vol->bdev, fat_sector, SECTOR_SIZE, (void*)KERNEL_VIRT_TO_PHYS(buf));
    uint32_t ret = (*(uint32_t*)(buf + entry_offset)) & 0x0FFFFFFF;

    if (alloc) RemoveKernelPages((uint64_t)buf, 1);

    return ret;
}

void ReadCluster(uint32_t cluster, fat32_internal_info_t* vol, void* buf) {
    uint32_t lba = ClusterToLBA(cluster, vol);
    vol->bdev->read(vol->bdev, lba, vol->sectors_per_cluster, (void*)KERNEL_VIRT_TO_PHYS(buf));
}

uint8_t lfn_checksum(const uint8_t* short_name) {
    uint8_t sum = 0;
    for (int i = 11; i > 0; i--)
        sum = ((sum & 1) ? 0x80 : 0x00) + (sum >> 1) + *short_name++;
    return sum;
}

int64_t ReadDirToBuf(fat32_internal_info_t* vol, uint32_t dir_cluster, fat32_dir_entry_t** out, uint32_t* out_count) {
    uint64_t cluster_count = 0, cluster = dir_cluster;
    void* next_buf = (void*) AddNonCachableKernelPages(1);

    while (!fat32_is_eoc(cluster)) {
        cluster_count++;
        cluster = FatNextCluster(cluster, vol, next_buf);
        memset(next_buf, 0, SECTOR_SIZE);
    }

    uint64_t entries_per_cluster = vol->bytes_per_sector * vol->sectors_per_cluster / 32;
    uint64_t total_entries = entries_per_cluster * cluster_count, total_entries_pages = (total_entries + PAGE_SIZE - 1) / PAGE_SIZE;

    fat32_dir_entry_t* buf = (fat32_dir_entry_t*) AddNonCachableKernelPages(total_entries_pages);

    cluster = dir_cluster;
    uint64_t offset = 0;

    while (!fat32_is_eoc(cluster)) {
        ReadCluster(cluster, vol, buf + offset);
        offset += entries_per_cluster;
        cluster = FatNextCluster(cluster, vol, next_buf);
    }

    *out = buf;
    *out_count = cluster_count;

    RemoveKernelPages((uint64_t)next_buf, 1);
        
    return 0;
}

void utf16_to_ascii(const uint16_t* utf16, char* ascii, int max_chars) {
    int i = 0;
    while (i < max_chars) {
        uint16_t c = utf16[i];

        if (c == 0x0000 || c == 0xFFFF) break;  

        if (c > 0x7F)
            ascii[i] = '?';   
        else
            ascii[i] = (char)(c & 0xFF); 

        i++;
    }
    ascii[i] = '\0';
}

void CollectLfnFragment(fat32_lfn_entry_t* lfn, char* out_buf) {
    uint8_t seq = (lfn->sequence_num & ~0x40) - 1;

    char fragment[14] = {0};
    uint16_t utf16[13];

    memcpy(utf16 + 0, lfn->name_part1, 5 * sizeof(uint16_t));
    memcpy(utf16 + 5, lfn->name_part2, 6 * sizeof(uint16_t));
    memcpy(utf16 + 11, lfn->name_part3, 2 * sizeof(uint16_t));

    // convert to ascii
    utf16_to_ascii(utf16, fragment, 13);

    // copy into the correct position in the output buffer
    memcpy(out_buf + seq * 13, fragment, 13);
}

int64_t ParseLFNs(fat32_lfn_entry_t* entry, char* out) {

    int64_t lfn_i = entry->sequence_num;
    if ((lfn_i & LAST_LFN) == LAST_LFN) lfn_i = lfn_i & ~(LAST_LFN);
    else return -3;
    int64_t i_83 = lfn_i;

    fat32_dir_entry_t* entry_83 = (fat32_dir_entry_t*)&entry[i_83];
    fat32_lfn_entry_t* lfn_entry;

    uint8_t checksum = lfn_checksum(entry_83->name);
    lfn_i--;

    while (0 <= lfn_i) {
        lfn_entry = &entry[lfn_i];

        if (checksum != lfn_entry->checksum) {
            memset(out, 0, MAX_FILENAME_FAT32);
            memcpy(out, entry_83->name, 8);
            return -1;
        }
        CollectLfnFragment(lfn_entry, out);
        lfn_i--;
    }
    return 0;
}

int64_t Fat32_LookUp(inode_t* parent_dir, dentry_t* dentry) {
    uint64_t pages_per_cluster = (parent_dir->sb->block_size + PAGE_SIZE - 1) / PAGE_SIZE,
     entries_per_cluster = parent_dir->sb->block_size / 32;

    char tmp_name[MAX_FILENAME_FAT32];
    memset(tmp_name, 0, MAX_FILENAME_FAT32);

    fat32_dir_entry_t* dir, *entry;
    uint32_t clusters_count = 0;
    ReadDirToBuf((fat32_internal_info_t*)parent_dir->sb->fs_info,(uint32_t)parent_dir->fs_data, &dir, &clusters_count);
    uint32_t pages_allocated = (clusters_count * parent_dir->sb->block_size + PAGE_SIZE - 1) / PAGE_SIZE;

    if (clusters_count == 0) {RemoveKernelPages((uint64_t)dir, pages_allocated);  return -2;}

    uint64_t i = 0;
    fat32_lfn_entry_t* lfn_entry;

    while (i * 32 < clusters_count * parent_dir->sb->block_size) {
        entry = &dir[i];
        uint8_t entry_status = *((uint8_t*)entry);

        if (entry_status == END_ENTRY) {RemoveKernelPages((uint64_t)dir, pages_allocated);  return 1;}
        if (entry_status == DELETED_ENTRY) {i++; continue;}
        if (entry->attributes == VOLUME_LABEL_ATTR) {i++; continue;}

        if (entry->attributes == LFN_ATTR) {
            lfn_entry = (fat32_lfn_entry_t*) entry;
            i += lfn_entry->sequence_num & ~(LAST_LFN);
            entry = &dir[i];
            ParseLFNs(lfn_entry, tmp_name);
        }
        else {
            memcpy(tmp_name, entry->name, 8);
            uint8_t i = 0;
            while (tmp_name[i] != SPACE_CHAR) i++; 
            tmp_name[i] = '\0';
        }

        if (dentry == NULL) {
            PrintDirEntry(entry, tmp_name);
            memset(tmp_name, 0, MAX_FILENAME_FAT32);
            i++;
            continue;
        }

        if (strcmp(tmp_name, dentry->name) == 0) {
            // IMPORTANT - Need to allocate and populate inode and make the dentry point to it here! - IMPORTANT
            RemoveKernelPages((uint64_t)dir, pages_allocated);
            return 0;
        }
        memset(tmp_name, 0, MAX_FILENAME_FAT32);
        i++;
    }
    RemoveKernelPages((uint64_t)dir, pages_allocated);
    return -1;
}

void PrintDirEntry(fat32_dir_entry_t* entry, char* name) {
    kprintf("Name: %s\n", name);
    kprintf("Attr: %x, File Size: %x\n", entry->attributes, entry->file_size);
}