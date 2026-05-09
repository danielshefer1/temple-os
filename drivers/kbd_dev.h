#pragma once
#include "includes.h"

// Register /dev/kbd (char major 13 minor 0). Exclusive open: only one
// reader at a time. While open, raw scancodes from the PS/2 IRQ are routed
// to this device's ring instead of the kernel TTY's translated input.
void kbd_dev_init(void);

// True iff some task has /dev/kbd open. The keyboard IRQ checks this to
// decide whether to dispatch into kbd_dev_input(scancode) or into the
// legacy console_tty translation path.
bool kbd_dev_active(void);

// IRQ-side producer: enqueue one raw scancode (0xE0 prefix bytes included
// — userspace term reassembles). Drops on overflow. Wakes one reader if any.
void kbd_dev_input(uint8_t scancode);
