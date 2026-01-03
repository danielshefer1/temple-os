#pragma once

#include "data_structs.h"
#include <stdint.h>
#include "E820.h"
#include "paging_bootstrap.h"
#include "E820.h"
#include "print_text.h"
#include "slab_alloc.h"

extern uint32_t __total_pages; 

typedef struct BuddyNode {
    bool free;
    uint32_t id;
    uint32_t buddy;
    uint32_t start;
    struct BuddyNode* next;
    struct BuddyNode* prev;
} BuddyNode;

typedef struct BuddyList
{
    uint32_t size;
    BuddyNode* head;
    struct BuddyList* next;
    struct BuddyList* prev;
} BuddyList;

BuddyList* AddList();
BuddyNode* AddNode();
BuddyList* InitBuddyList(uint32_t start, uint32_t size);
void PrintBuddyList(BuddyList* list);
void PrintBuddyNode(BuddyNode* node);
uint32_t GetLargestBit(uint32_t size);
uint32_t NumOfActiveBits(uint32_t size);
void* brk(BuddyList* list, uint32_t size);