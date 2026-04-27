#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"

#include "blocks_buffer.h"
#include "ext2_helpers.h"
#include "rtc.h"

int64_t EXT2Read(file_t* file, void* buf, uint64_t size);
int64_t EXT2Write(file_t* file, const void* buf, uint64_t size);
int64_t EXT2Truncate(file_t* file, uint64_t new_size);
int64_t EXT2ReadDir(file_t* file, dentry_t* out);
int64_t EXT2Open(inode_t* inode, file_t* file);
int64_t EXT2Close(file_t* file);
int64_t EXT2Flush(file_t* file);
int64_t EXT2Ioctl(file_t* file, uint64_t cmd, void* arg);
int64_t EXT2Seek(file_t* file, int64_t offset, int64_t whence);

extern file_ops_t ext2_file_ops;