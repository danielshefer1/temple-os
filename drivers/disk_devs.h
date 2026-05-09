#pragma once

// Walks the AHCI block-device list and the MBR partition list, registers
// each whole disk and partition with devfs (major 8, Linux sd convention),
// and creates the corresponding /dev/sd[a-z][1-15] nodes on the mounted
// root filesystem. Call once during start() after vfs_mount_root().
void disk_devs_init(void);
