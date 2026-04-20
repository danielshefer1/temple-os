#pragma once

#include "types.h"
#include "includes.h"
#include "defintions.h"
#include "ext2_sb_ops.h"
#include "ext2_helpers.h"
#include "math.h"
#include "memory.h"
#include "string.h"

int64_t EXT2Lookup(inode_t* dir, dentry_t* dentry);
int64_t EXT2Create(inode_t* dir, dentry_t* dentry, uint64_t permissions);
int64_t EXT2Mkdir(inode_t* dir, dentry_t* dentry, uint64_t permissions);
int64_t EXT2Unlink(inode_t* dir, dentry_t* dentry);
int64_t EXT2Rmdir(inode_t* dir, dentry_t* dentry);
int64_t EXT2Rename(inode_t* old_dir, dentry_t* old_dentry, inode_t* new_dir, dentry_t* new_dentry);
int64_t EXT2HardLink(inode_t* dir, inode_t* existing_inode, dentry_t* new_dentry);
int64_t EXT2SymLink(inode_t* dir, dentry_t* dentry, const char* target);
int64_t EXT2ReadLink(inode_t* inode, char* buf, uint64_t size);