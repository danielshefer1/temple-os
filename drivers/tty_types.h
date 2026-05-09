#pragma once
#include "includes.h"
#include "lock_types.h"

#define TTY_BUF_SIZE 512

#define TTY_FLAG_ICANON  (1u << 0)
#define TTY_FLAG_ECHO    (1u << 1)
#define TTY_FLAG_ISIG    (1u << 2)

#define TTY_IOCTL_SET_RAW        0x100
#define TTY_IOCTL_SET_COOKED     0x101
#define TTY_IOCTL_SET_FOREGROUND 0x102   // legacy alias for TIOCSPGRP, takes pgid in arg
#define TTY_IOCTL_TIOCSPGRP      0x103   // set foreground process group (arg = pgid)
#define TTY_IOCTL_TIOCGPGRP      0x104   // get foreground process group (arg = uint64_t*)
#define TTY_IOCTL_TIOCGWINSZ     0x105   // get terminal geometry (arg = struct winsize*)

// Linux-compatible struct: layout is (row, col, xpixel, ypixel) — uint16 each.
typedef struct winsize {
    uint16_t ws_row;
    uint16_t ws_col;
    uint16_t ws_xpixel;
    uint16_t ws_ypixel;
} winsize_t;

struct task_t;
struct file_ops_t;

typedef struct tty_t {
    spinlock_t input_lock;
    char       buf[TTY_BUF_SIZE];
    uint64_t   head;        // monotonic; index = head % TTY_BUF_SIZE
    uint64_t   tail;        // monotonic
    uint32_t   flags;
    bool       shift_pressed;
    // Foreground process group ID. signal-on-Ctrl+C is delivered to every
    // task whose pgid matches this. 0 = no foreground (signal is dropped).
    uint64_t   pgrp;
    struct task_t* read_waiter;
    struct task_t* read_waiter_tail;
    struct file_ops_t* fops;
} tty_t;
