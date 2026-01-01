#pragma once

#include "data_structs.h"
#include <stdint.h>
#include "E820.h"
#include "paging_bootstrap.h"
#include "E820.h"
#include "print_text.h"

extern uint32_t __total_pages; 

typedef struct buddyNode {
    bool free;
    uint32_t id;
    uint32_t buddy;
    uint32_t start;
    struct buddyNode* next;
    struct buddyNode* prev;
} buddyNode;

typedef struct BuddyList
{
    uint32_t size;
    buddyNode* head;
    struct BuddyList* next;
    struct BuddyList* prev;
} BuddyList;

BuddyList* InitBuddyList(uint32_t stack_size, E820Entry* entry);
void PrintBuddyList(BuddyList* list);
void PrintBuddyListH(BuddyList* list);
void PrintBuddyNode(buddyNode* node);
void PrintBuddyNodeH(buddyNode* node);
