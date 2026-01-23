#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "vga.h"
#include "str_ops.h"
#include "dcache.h"

void InitVFS();
void PrintVFS_Dentry(vfs_dentry_t* dentry, int32_t tab_number);
void PrintVFS_Root();
void PrintVFS_Helper(vfs_dentry_t* dentry, int32_t tab_number);
void PrintVFS_Inode(vfs_inode_t* inode);
vfs_dentry_t* FindDentry(vfs_dentry_t* start_dentry, char* path);
vfs_dentry_t* CreateDentry(vfs_inode_t* inode, char* name);
void AddDentryToParent(vfs_dentry_t* parent, vfs_dentry_t* node);
const char* GetNextSegment(const char* path, char* buffer, uint32_t max_len);
vfs_dentry_t* VFS_CreateDentry(char* name, char* parent_name, uint32_t type, vfs_dentry_t* start_dentry);
vfs_dentry_t* VFS_Link(vfs_inode_t* inode, char* name, char* parent_name, vfs_dentry_t* start_dentry);
vfs_dentry_t* VFS_Mount(char* name, char* parent_name, vfs_dentry_t* start_dentry, vfs_dentry_t* mounted_dir);
vfs_dentry_t* SearchChildren(vfs_dentry_t* child, char* segment);