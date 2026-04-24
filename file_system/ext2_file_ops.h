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