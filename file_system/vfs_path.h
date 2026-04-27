#pragma once

#include "types.h"

// Resolve `path` to an existing dentry.
int64_t vfs_namei(const char* path, dentry_t** out);

// Resolve everything before the last '/'; copy the final component into leaf_out.
int64_t vfs_namei_parent(const char* path,
                         dentry_t** parent_out,
                         char* leaf_out, uint64_t leaf_cap);

// Walk a relative path starting at `start`. Used internally and by syscall layer
// when a cwd is supplied. Caller passes NULL `start` to anchor at vfs_root.
int64_t vfs_path_walk(dentry_t* start, const char* rel, dentry_t** out);
