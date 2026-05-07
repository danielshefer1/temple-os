#pragma once
#include "includes.h"
#include "lock_types.h"

#define TTY_BUF_SIZE 512

#define TTY_FLAG_ICANON  (1u << 0)
#define TTY_FLAG_ECHO    (1u << 1)
#define TTY_FLAG_ISIG    (1u << 2)

#define TTY_IOCTL_SET_RAW        0x100
#define TTY_IOCTL_SET_COOKED     0x101
#define TTY_IOCTL_SET_FOREGROUND 0x102

struct task_t;
struct file_ops_t;

typedef struct tty_t {
    spinlock_t input_lock;
    char       buf[TTY_BUF_SIZE];
    uint64_t   head;        // monotonic; index = head % TTY_BUF_SIZE
    uint64_t   tail;        // monotonic
    uint32_t   flags;
    bool       shift_pressed;
    struct task_t* foreground;
    struct task_t* read_waiter;
    struct task_t* read_waiter_tail;
    struct file_ops_t* fops;
} tty_t;
