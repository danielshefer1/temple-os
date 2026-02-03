#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "math.h"
#include "slab_alloc.h"
#include "paging.h"

void PrintBuddyNode(buddy_node_t* node);
void PrintBuddyBin(uint64_t start_order, uint64_t end_order);
buddy_node_t* CreateBuddyNode(void* address, uint64_t order);
void InitBuddyAlloc(uint64_t start, uint64_t size);
void* RequestBuddy(uint64_t size);
void* GetBuddyAddress(void* address, uint64_t order);
void InsertSortedBuddyNode(buddy_bin_t* bin, buddy_node_t* node, bool free_list);
void RemoveBuddyNode(buddy_bin_t* bin, void* address, bool free_list);
void* SplitNode(buddy_node_t* node, uint64_t target_order);
void FreeBuddy(void* address);
void FillPageDirectory(void* addr, uint64_t size);
uint64_t FindLowest();