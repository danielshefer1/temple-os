#pragma once
#include "pty_types.h"
#include "vfs_types.h"

// One-time init at boot. Zeros the table and registers the ptmx + slave
// fops with devfs. Slave devfs entries cover minors 0..PTY_MAX_PAIRS-1.
void pty_init(void);

// Look up a pair by index; NULL if unallocated. Used by /dev/pts/N opens
// and by sanity assertions.
pty_pair_t* pty_get(uint16_t index);

// Discard the s2m (slave-to-master) ring contents on the pty backing the
// dying task's controlling terminal, but only if `pgid` matches the pty's
// foreground pgrp. Called from the signal-kill path so a foreground task
// taken out by SIGINT etc. doesn't leave kernel-buffered output to drain
// for tens of seconds. No-op if `f` isn't a pty slave fd.
void pty_drop_fg_output(file_t* f, uint64_t pgid);
