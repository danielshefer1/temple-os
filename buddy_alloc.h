#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "print_text.h"
#include "math_ops.h"
#include "slab_alloc.h"

void PrintBuddyNode(BuddyNode* node);
void PrintBuddyBin();
BuddyNode* CreateBuddyNode(void* address);
void InitBuddyAlloc(uint32_t start, uint32_t size);