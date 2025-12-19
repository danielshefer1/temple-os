#pragma once
#include <stdint.h>

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