#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"
#include "math.h"
#include "slab_alloc.h"

void PrintBuddyNode(buddy_node_t* node);
void PrintBuddyBin(uint64_t start_order, uint64_t end_order, bool user);
buddy_node_t* CreateBuddyNode(void* address, uint64_t order);
void InitUserBuddyAlloc(e820_info_t* info);
void InitKernelBuddyAlloc(uint64_t start, uint64_t end);
void* RequestBuddy(uint64_t size, bool user);
void* GetBuddyAddress(void* address, uint64_t order);
void InsertSortedBuddyNode(buddy_bin_t* bin, buddy_node_t* node, bool free_list);
void RemoveBuddyNode(buddy_bin_t* bin, void* address, bool free_list);
void* SplitNode(buddy_node_t* node, uint64_t target_order, buddy_bin_t* bins);
void FreeBuddy(void* address, bool user);
uint64_t FindLowest(buddy_bin_t* bins);