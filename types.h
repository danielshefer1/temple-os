#pragma once
#include "includes.h"

typedef struct BuddyNode {
    void* address;
    struct BuddyNode* next;
} BuddyNode;

typedef struct BuddyBin {
    BuddyNode* head_free;
    BuddyNode* head_used;
} BuddyBin;

typedef struct pte_t {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} pte_t;

typedef struct pde_t {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} pde_t;

typedef struct Slab
{
    void* start;
    uint32_t num_slots;
    uint32_t free_count;
    struct Slab* next;
    uint32_t bitmap_size;
    uint32_t bitmap[];
} Slab;

typedef struct 
{
    uint32_t size;
    Slab* full_slabs;
    Slab* partial_slabs;
    Slab* empty_slabs;
} Cache;

typedef struct E820Entry {
    uint32_t base_low;    // Lower 32 bits of base
    uint32_t base_high;   // Upper 32 bits of base  
    uint32_t length_low;  // Lower 32 bits of length
    uint32_t length_high; // Upper 32 bits of length
    uint32_t type;
} E820Entry;

typedef struct E820Info {
    uint32_t signature;
    uint32_t num_entries;
    E820Entry* entries;
    uint32_t address;
} E820Info;

typedef struct Tuple {
    uint32_t first;
    uint32_t second;
} Tuple;

typedef struct intNode {
    uint32_t val;
    struct intNode* next;
} intNode;

typedef struct tupleNode {
    Tuple val;
    struct tupleNode* next;
} tupleNode;