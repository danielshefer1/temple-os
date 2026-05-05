#pragma once
#include "includes.h"
#include "lock_types.h"

typedef struct buffer_node {
    bool is_valid;
    bool is_dirty;

    uint32_t block_number;
    uint8_t *data;
    uint64_t ref_count;

    struct buffer_node *prev;
    struct buffer_node *next;

    struct buffer_node *hash_next;

    mutex_t mutex;
} buffer_node_t;

typedef struct buffer_cache {
    buffer_node_t** hash_table;
    buffer_node_t* lru_head;
    buffer_node_t* lru_tail;

    uint64_t capacity;
    uint64_t size;
    uint64_t hash_table_length;

    mutex_t lock;
} buffer_cache_t;
