#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vfs.h"
#include "paging.h"
#include "string.h"

int64_t Fat32MountRoot(fat32_internal_info_t* info);
int64_t Fat32Mount(fat32_internal_info_t* info, partition_device_t* part);
int64_t Fat32_LookUp(fat32_internal_info_t* vol, uint32_t dir_cluster, dentry_t* dentry);
void PrintDirEntry(fat32_dir_entry_t* entry, char* name);