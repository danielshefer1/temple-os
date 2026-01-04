#pragma once

#include "data_structs.h"
#include <stdint.h>
#include "E820.h"
#include "paging_bootstrap.h"
#include "E820.h"
#include "print_text.h"
#include "math_ops.h"

extern uint32_t __total_pages; 

typedef struct BuddyNode {
    bool free;
    void* start;
    struct BuddyNode* next;
    struct BuddyNode* prev;
    struct BuddyList* list;
} BuddyNode;

typedef struct BuddyList
{
    uint32_t size;
    BuddyNode* head;
    struct BuddyList* next;
    struct BuddyList* prev;
} BuddyList;

void DeleteGarbage();
void InitBuddyAlloc(uint32_t start, uint32_t size);
void GoToBack();
BuddyNode* InsertNodeSorted(BuddyNode* node, BuddyNode* head);
void DeleteNode(BuddyNode* node);
BuddyNode* SearchFreeNode(BuddyList* list);
void* RequestBlock(uint32_t size);
void PrintList();