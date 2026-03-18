#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vfs.h"
#include "paging.h"

int64_t Fat32MountRoot(fat32_internal_info_t* info);
int64_t Fat32Mount(fat32_internal_info_t* info, partition_device_t* part);
int64_t Fat32_LookUp(fat32_internal_info_t* info, uint32_t dir_cluster, char* name, fat32_dir_entry_t* entry);
void PrintEntry(fat32_dir_entry_t* entry);