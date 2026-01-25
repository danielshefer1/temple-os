#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "vga.h"
#include "str_ops.h"
#include "dcache.h"

void InitVFS();
void PrintVFS_Dentry(dentry_t* dentry, int32_t tab_number);
void PrintVFS_Root();
void PrintVFS_Helper(dentry_t* dentry, int32_t tab_number);
void PrintVFS_Inode(inode_t* inode);
dentry_t* FindDentry(dentry_t* cwd, char* path);
dentry_t* CreateDentry(inode_t* inode, char* name);
void AddDentryToParent(dentry_t* parent, dentry_t* node);
const char* GetNextSegment(const char* path, char* buffer, uint32_t max_len);
dentry_t* VFS_CreateDentry(char* name, char* parent_name, uint32_t type, dentry_t* cwd);
dentry_t* VFS_HardLink(char* name, char* parent_name, dentry_t* cwd, inode_t* inode, char* inode_path);
dentry_t* VFS_Mount(char* name, char* parent_name, dentry_t* cwd, dentry_t* mounted_dir);
dentry_t* VFS_SysLink(char* name, char* parent_name, dentry_t* cwd, char* syslink_name);
void VFS_RemoveDentry(dentry_t* dentry);
void RemoveDentryFromParent(dentry_t* dentry);
dentry_t* SearchChildren(dentry_t* child, char* segment);
bool IsValidSysLink(dentry_t* dentry);
void GetTypeString(uint32_t type_idx, char* type);