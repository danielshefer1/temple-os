#pragma once
#include "includes.h"

typedef struct timed_key_t {
    uint64_t time;
    char c;
} timed_key_t;

typedef struct input_buffer_t {
    struct timed_key_t* buffer;
    uint64_t size;
    uint64_t head;
    uint64_t tail;
} input_buffer_t;
