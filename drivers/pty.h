#pragma once
#include "pty_types.h"
#include "vfs_types.h"

// One-time init at boot. Zeros the table and registers the ptmx + slave
// fops with devfs. Slave devfs entries cover minors 0..PTY_MAX_PAIRS-1.
void pty_init(void);

// Look up a pair by index; NULL if unallocated. Used by /dev/pts/N opens
// and by sanity assertions.
pty_pair_t* pty_get(uint16_t index);
