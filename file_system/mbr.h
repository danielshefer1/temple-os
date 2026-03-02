#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"
#include "global.h"
#include "paging.h"
#include "vga.h"
#include "slab_alloc.h"
#include "vga.h"

void ParseDevicesMbrs();
void PrintParitions();
void InsertDisksAndPartsInVFS();