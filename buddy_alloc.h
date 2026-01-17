#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "math_ops.h"
#include "slab_alloc.h"
#include "paging.h"

void PrintBuddyNode(BuddyNode* node);
void PrintBuddyBin(uint32_t start_order, uint32_t end_order);
BuddyNode* CreateBuddyNode(void* address, uint32_t order);
void InitBuddyAlloc(uint32_t start, uint32_t size);
void* RequestBuddy(uint32_t size);
void* GetBuddyAddress(void* address, uint32_t order);
void InsertSortedBuddyNode(BuddyBin* bin, BuddyNode* node, bool free_list);
void RemoveBuddyNode(BuddyBin* bin, void* address, bool free_list);
void* SplitNode(BuddyNode* node, uint32_t target_order);
void FreeBuddy(void* address);
void FillPageDirectory(void* addr, uint32_t size);
uint32_t FindLowest();