#include "devfs.h"
#include "string.h"
#include "vfs_defs.h"

// Boot-time-populated table; not protected by a lock because every caller
// of devfs_register* runs from the BSP during start(), and devfs_lookup is
// pure read.
static devfs_entry_t devices[DEVFS_MAX_DEVICES];

void devfs_init(void) {
    memset(devices, 0, sizeof(devices));
}

static int64_t devfs_register(bool is_block, uint32_t major, uint32_t minor,
                              file_ops_t* fops, void* token) {
    if (fops == NULL) return -EINVAL;
    int64_t free_slot = -1;
    for (int64_t i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (devices[i].fops == NULL) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (devices[i].is_block == is_block &&
            devices[i].major == major &&
            devices[i].minor == minor) {
            return -EBUSY;
        }
    }
    if (free_slot < 0) return -ENOMEM;
    devices[free_slot].major    = major;
    devices[free_slot].minor    = minor;
    devices[free_slot].is_block = is_block;
    devices[free_slot].fops     = fops;
    devices[free_slot].token    = token;
    return 0;
}

int64_t devfs_register_char(uint32_t major, uint32_t minor, file_ops_t* fops, void* token) {
    return devfs_register(false, major, minor, fops, token);
}

int64_t devfs_register_block(uint32_t major, uint32_t minor, file_ops_t* fops, void* token) {
    return devfs_register(true, major, minor, fops, token);
}

devfs_entry_t* devfs_lookup(bool is_block, uint32_t major, uint32_t minor) {
    for (int64_t i = 0; i < DEVFS_MAX_DEVICES; i++) {
        if (devices[i].fops == NULL) continue;
        if (devices[i].is_block == is_block &&
            devices[i].major == major &&
            devices[i].minor == minor) {
            return &devices[i];
        }
    }
    return NULL;
}
