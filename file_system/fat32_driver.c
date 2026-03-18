#include "fat32_driver.h"

bool fat32_is_eoc(uint32_t val) {return val >= 0x0FFFFFF8;}
bool fat32_is_free(uint32_t val) {return val == 0x00000000;}
bool fat32_is_bad(uint32_t val) {return val == 0x0FFFFFF7;}
bool fat32_is_valid(uint32_t val) {return val >= 0x00000002 && val <= 0x0FFFFFEF;}

void CopyBPB_IntoInfo(fat32_internal_info_t* info, fat32_bpb_t* bpb, partition_device_t* part) {
    info->bdev = part->physical_device;
    info->bytes_per_sector = bpb->bytes_per_sector;
    info->sectors_per_cluster = bpb->sectors_per_cluster;
    info->reserved_sectors = bpb->reserved_sectors;
    info->number_of_fats = bpb->fat_count;
    info->fat_size = bpb->fat_size_32;
    info->root_cluster = bpb->root_cluster;

    info->fat_start_lba = part->start_lba + bpb->reserved_sectors;

    info->data_start_lba = part->start_lba + bpb->reserved_sectors
                           + (bpb->fat_count * bpb->fat_size_32);

    info->total_clusters = (bpb->total_sectors_32
                                 - bpb->reserved_sectors
                                 - (bpb->fat_count * bpb->fat_size_32))
                                 / bpb->sectors_per_cluster;

    info->free_clusters      = 0;
    info->last_alloc_cluster = 0;
}

int64_t Fat32MountRoot(fat32_internal_info_t* info) {
    if (parts_head == NULL) return -1;
    fat32_bpb_t* buffer = (fat32_bpb_t*)AddNonCachableKernelPages(1);

    partition_device_node_t* p = parts_head;
    partition_device_t* part;

    while (p != NULL) {
        part = p->value;
        part->physical_device->read(part->physical_device, part->start_lba, SECTOR_SIZE, (void*) KERNEL_VIRT_TO_PHYS(buffer));

        if (strncmp(buffer->volume_label, ROOT_LABEL, ROOT_LABEL_LENGTH) == 0) {
            CopyBPB_IntoInfo(info, buffer, part);
            RemoveKernelPages((uint64_t)buffer, 1);
            return 0;
        }
        p = p->next;
    }
    RemoveKernelPages((uint64_t)buffer, 1); 
    return 1;
}

int64_t Fat32Mount(fat32_internal_info_t* info, partition_device_t* part) {

    fat32_bpb_t* buffer = (fat32_bpb_t*)AddNonCachableKernelPages(1);
    part->physical_device->read(part->physical_device, part->start_lba, SECTOR_SIZE, (void*)KERNEL_VIRT_TO_PHYS(buffer));

    if (strncmp(buffer->volume_label, ROOT_LABEL, ROOT_LABEL_LENGTH) == 0) {
        CopyBPB_IntoInfo(info, buffer, part);
        RemoveKernelPages((uint64_t)buffer, 1);
        return 0;
    }
    RemoveKernelPages((uint64_t)buffer, 1); 
    return 1;
}

uint32_t ClusterToLBA(uint32_t cluster, fat32_internal_info_t* info) {
    return info->data_start_lba + (cluster - 2)*info->sectors_per_cluster;
}

uint32_t FatNextCluster(uint32_t cluster, fat32_internal_info_t* info) {
    uint32_t fat_offset   = cluster * 4;
    uint32_t fat_sector   = info->fat_start_lba + (fat_offset / info->bytes_per_sector);
    uint32_t entry_offset = fat_offset % info->bytes_per_sector;

    void* buf = (void*)AddNonCachableKernelPages(1);
    info->bdev->read(info->bdev, fat_sector, SECTOR_SIZE, buf);
    uint32_t ret = (*(uint32_t*)(buf + entry_offset)) & 0x0FFFFFFF;
    RemoveKernelPages((uint64_t)buf, 1);

    return ret;
}

void ReadCluster(uint32_t cluster, fat32_internal_info_t* info, void* buf) {
    uint32_t lba = ClusterToLBA(cluster, info);
    info->bdev->read(info->bdev, lba, info->sectors_per_cluster, (void*)KERNEL_VIRT_TO_PHYS(buf));
}

int64_t Fat32_LookUp(fat32_internal_info_t* info, uint32_t dir_cluster, char* name, fat32_dir_entry_t* entry) {
    uint64_t pages_per_cluster = (info->sectors_per_cluster * info->bytes_per_sector + PAGE_SIZE - 1) / PAGE_SIZE,
     entries_per_cluster = info->sectors_per_cluster * info->bytes_per_sector / 32;
    fat32_dir_entry_t* buf = (fat32_dir_entry_t*) AddNonCachableKernelPages(pages_per_cluster);
    uint8_t tmp = 0;

    uint32_t cluster = dir_cluster;
    bool dir_ended = false;

    while (!fat32_is_eoc(cluster)) {
        ReadCluster(cluster, info, buf);

        for (uint32_t i = 0; i <= entries_per_cluster; i++) {
            uint8_t entry_status = ((uint8_t*)&buf[i])[0];

            if (entry_status == 0x00) {dir_ended=true; break;}
            if (entry_status == 0xE5) continue;

            if (entry_status == 0x0F) continue;  
            if (buf[i].attributes == 0x08) continue;  
            PrintEntry(&buf[i]);

            if (name == NULL) continue;
        }
        if (dir_ended) break;

        cluster = FatNextCluster(cluster, info);
    }
    return 0;
}

void PrintEntry(fat32_dir_entry_t* entry) {
    char name[9]  = {0};  
    char ext[4]   = {0};  

    memcpy(name, entry->name, 8);
    memcpy(ext,  entry->ext,  3);

    for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
    for (int i = 2; i >= 0 && ext[i]  == ' '; i--) ext[i]  = '\0';

    kprintf("Name: %s.%s\n", name, ext);
    kprintf("Attr: %x, File Size: %x\n", entry->attributes, entry->file_size);
}