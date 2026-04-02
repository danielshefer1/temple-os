#pragma once

#include "types.h"
#include "includes.h"
#include "defintions.h"
#include "ext2_sb_ops.h"

int64_t EXT2Lookup(inode_t* dir, dentry_t* dentry);