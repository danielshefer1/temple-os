#pragma once

// In-memory block device used to exercise the block-device side of devfs.
// Allocates a small backing buffer at boot and registers as block (1, 0)
// so /dev/ram0 can be opened for read/write/seek. Call once during start()
// after devfs_init.
void ram_block_init(void);
