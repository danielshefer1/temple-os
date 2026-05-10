#pragma once

// Subset of -errno values userspace cares about. The kernel returns these
// as negative longs from syscalls; callers compare via `if (r == -EINTR)`
// or `if (r < 0)`.

#define EINTR              4
