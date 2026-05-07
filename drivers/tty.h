#pragma once
#include "tty_types.h"
#include "vfs_types.h"

extern tty_t console_tty;

// One-time init. Sets cooked + echo + isig defaults; zeroes the ring.
void tty_init(tty_t* tty);

// IRQ-side producer: feed one byte from the keyboard. Handles Ctrl+C
// (forwards SIGINT to the foreground task), backspace erase, echo, and
// wakes any blocked reader.
void tty_input_byte(tty_t* tty, char c);

// Allocate a refcounted file_t for `tty`. ref_count = 1.
file_t* tty_open(tty_t* tty);

// Drop a foreground reference if `t` was the foreground task. Safe to call
// from task teardown paths.
void tty_drop_task(struct task_t* t);
