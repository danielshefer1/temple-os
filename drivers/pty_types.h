#pragma once
#include "includes.h"
#include "lock_types.h"
#include "pty_defs.h"

struct task_t;

// One pseudo-terminal pair. Two rings so master and slave can talk without
// stepping on each other:
//   m2s — keystrokes from master into slave (typed at the userspace term).
//         Slave reads after the line discipline cooks them.
//   s2m — output from program-running-on-slave back to master, which the
//         userspace term renders to the framebuffer. Always raw.
//
// flags reuses the TTY_FLAG_* bits (drivers/tty_types.h) so the same line
// discipline (drivers/tty_ldisc.c) can drive both.
typedef struct pty_pair_t {
    spinlock_t lock;

    // master -> slave (input ring); cooked through line discipline on read.
    char       m2s[PTY_BUF_SIZE];
    uint64_t   m2s_head;
    uint64_t   m2s_tail;
    struct task_t* s_read_waiter;
    struct task_t* s_read_waiter_tail;

    // slave -> master (output ring); raw bytes, drained by master read.
    char       s2m[PTY_BUF_SIZE];
    uint64_t   s2m_head;
    uint64_t   s2m_tail;
    struct task_t* m_read_waiter;
    struct task_t* m_read_waiter_tail;
    // Slave writers parked because s2m is full. master_read pops one per
    // drain; master close wakes them all so they fall out with -EPIPE.
    struct task_t* s_write_waiter;
    struct task_t* s_write_waiter_tail;

    uint32_t   flags;            // ICANON / ECHO / ISIG (same bits as tty_t)
    uint64_t   pgrp;             // slave's foreground process group
    uint16_t   rows, cols;       // for TIOCGWINSZ; userspace term sets via TIOCSWINSZ
    uint16_t   xpixel, ypixel;

    bool       in_use;           // slot allocated
    bool       master_open;
    bool       slave_open;
    bool       slave_ever_opened; // sticky: set on first slave open
    bool       locked;           // unlockpt() flips this off
    uint16_t   index;            // /dev/pts/N
} pty_pair_t;
