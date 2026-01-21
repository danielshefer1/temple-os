#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "vga.h"
#include "str_ops.h"

void InitVFS();
void PrintVFSNode(VFSNode* node);
void PrintVFS();
void PrintVFSAttr(VFSAttr* attr);
void AddVFSNode(uint32_t type, uint32_t perm, uint32_t owner_id, uint32_t group_id, char* name, char* parent_name);
