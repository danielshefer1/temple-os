#pragma once

#include "types.h"
#include "includes.h"
#include "defintions.h"
#include "ext2_sb_ops.h"
#include "ext2_helpers.h"
#include "math.h"
#include "string.h"

int64_t EXT2Lookup(inode_t* dir, dentry_t* dentry);
int64_t EXT2Create(inode_t* dir, dentry_t* dentry, uint64_t permissions);
int64_t EXT2Mkdir(inode_t* dir, dentry_t* dentry, uint64_t permissions);
int64_t EXT2Unlink(inode_t* dir, dentry_t* dentry);