#include "mbr.h"

void ParseMbr(uint8_t* buffer, block_device_t* dev) {
    mbr_t* mbr = (mbr_t*)buffer;
    partition_device_t* new_part;
    partition_device_node_t* new_node;
    if (mbr->signature != 0xAA55) {
        kprintf("Error: MBR Signature mismatch (Expected 0xAA55, got %x)\n", mbr->signature);
        return;
    }


    for (int i = 0; i < 4; i++) {
        mbr_partition_t* part = &mbr->partitions[i];

        if (part->sector_count == 0) continue;
        if (part->attributes != 0x80) continue;

        new_part = (partition_device_t*) kmalloc(sizeof(partition_device_t));
        new_node = (partition_device_node_t*) kmalloc(sizeof(partition_device_node_t));


        new_part->partition_type = part->partition_type;
        new_part->sector_count = part->sector_count;
        new_part->start_lba = part->lba_start;
        new_part->physical_device = dev;

        new_node->value = new_part;
        new_node->next = parts_head;
        parts_head = new_node;
    }
}

void ParseDevicesMbrs() {
    if (devices_head == NULL) {
        kprintf("You need to find devices_head using AhciInit() before you call this func!\n");
        return;
    }

    block_device_node_t* p = devices_head;
    block_device_t* dev;

    void* buffer = (void*) AddNonCachableKernelPages(1);

    while (p != NULL) {
        dev = p->value;
        dev->read(dev, 0, 1, (void*)KERNEL_VIRT_TO_PHYS(buffer));
        ParseMbr((uint8_t*)buffer, dev);
        p = p->next;
        memset(buffer, 0, 512);
    }
}

void InsertDisksAndPartsInVFS() {
    if (parts_head == NULL) ParseDevicesMbrs();
    if (parts_head == NULL) {
        kprintf("ParseDevicesMbr() doesn't work as expected, aborting!\n");
        return;
    }

    dentry_t* dev_dir = NULL/*(VFS_CreateDentry("dev", "/", VFS_DIRECTORY, NULL)*/, *part_den;

    block_device_node_t* dev_p = devices_head;
    partition_device_node_t* part_p = parts_head;

    block_device_t* dev;
    partition_device_t* part = part_p->value;

    int64_t idx = 0;
    block_device_t* curr_dev = part->physical_device;
    char part_name[256];
    uint64_t curr_dev_len = strlen(part->physical_device->name);
    cpystr(part->physical_device->name, part_name);


    while (dev_p != NULL) {
        dev = dev_p->value;
        //VFS_CreateDentry(dev->name, ".", DISK, dev_dir);
        dev_p = dev_p->next;
    }
    while (part_p != NULL) {
        part = part_p->value;
        if (part->physical_device != curr_dev) {
            idx = 0;
            curr_dev_len = strlen(part->physical_device->name);
            cpystr(part->physical_device->name, part_name);
        } 
        idx++;
        part_name[curr_dev_len] = idx + '0';

        part_den = NULL; //VFS_CreateDentry(part_name, ".", VFS_PARTITION, dev_dir);
        part_den->inode->driver_data = part;
        part_p = part_p->next;
    }
}

void PrintParitions() {
    if (parts_head == NULL) return;

    partition_device_node_t* p = parts_head;
    partition_device_t* part;
    int64_t idx = -1;
    block_device_t* curr_dev = p->value->physical_device;

    while (p != NULL) {
        part = p->value;
        if (part->physical_device != curr_dev) {
            idx = -1;
            curr_dev = part->physical_device;
        }
        idx++;
        kprintf("Partition Name: %s%d\n", part->physical_device->name, idx);
        kprintf("Start LBA: %d\t", part->start_lba);
        kprintf("Sector Count: %d\t", part->sector_count);
        kprintf("Partition Type: %x\n", part->partition_type);
        p = p->next;
    }
}