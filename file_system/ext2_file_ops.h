#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"

#include "blocks_buffer.h"
#include "ext2_helpers.h"

int64_t EXT2Read(file_t* file, void* buf, uint64_t size);