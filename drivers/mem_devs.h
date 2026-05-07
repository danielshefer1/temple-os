#pragma once

// Initialize and register the trivial memory char devices: /dev/null (1, 3)
// and /dev/zero (1, 5). Call once during start() after devfs_init.
void mem_devs_init(void);
