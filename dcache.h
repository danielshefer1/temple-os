#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"
#include "slab_alloc.h"
#include "str_ops.h"
#include "vga.h"

void dCachePut(dentry_t* dentry);
void dCacheRemove(dentry_t* dentry);
dentry_t* dCacheLookup(dentry_t* parent, const char* name);