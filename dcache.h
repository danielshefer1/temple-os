#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"
#include "slab_alloc.h"
#include "str_ops.h"
#include "vga.h"

void dCachePut(vfs_dentry_t* dentry);
void dCacheRemove(vfs_dentry_t* dentry);
vfs_dentry_t* dCacheLookup(vfs_dentry_t* parent, const char* name);