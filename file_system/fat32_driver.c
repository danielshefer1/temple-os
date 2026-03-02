#include "fat32_driver.h"

dentry_t* ProbeForData() {
    dentry_t* dev = NULL/*FindDentry(NULL, "/dev")*/, *d_p;
    if (dev == NULL) {
        kprintf("You need to parse the disks and parts first!");
        return NULL;
    }  

    d_p = dev->children;

    uint8_t buffer[512];
    partition_device_t* part;
    fat32_bpb_t* bpb;

    while (d_p != NULL) {
        if (d_p->inode->type != VFS_PARTITION) {
            d_p = d_p->next;
            continue;
        }  

        part = (partition_device_t*) d_p->inode->driver_data;
        part->physical_device->read(part->physical_device, part->start_lba, 1, (void*) KERNEL_VIRT_TO_PHYS(buffer));
        bpb = (fat32_bpb_t*) buffer;
        if (strncmp(bpb->volume_label, ROOT_LABEL, ROOT_LABEL_LENGTH) == 0) return d_p;

        memset(buffer, 0, 512);
        d_p = d_p->next;
    }

    return NULL;
}