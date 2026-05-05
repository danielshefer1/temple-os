#pragma once
#include "includes.h"

typedef struct buddy_node_t {
    bool free;
    void* address;
    uint64_t order;
    struct buddy_node_t* next;
} buddy_node_t;

typedef struct buddy_bin_t {
    buddy_node_t* head_free;
    buddy_node_t* head_used;
} buddy_bin_t;
