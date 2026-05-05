#pragma once
#include "includes.h"

typedef struct spinlock_t {
    volatile uint64_t locked;
} spinlock_t;

typedef struct mutex_t {
    spinlock_t guard;            // protects fields below
    volatile uint64_t locked;    // 0 = free, 1 = held
    struct task_t* owner;        // current holder; NULL when free
    struct task_t* wait_head;    // FIFO of blocked tasks linked via task_t::next
    struct task_t* wait_tail;
} mutex_t;
