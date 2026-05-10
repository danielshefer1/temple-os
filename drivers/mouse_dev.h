#pragma once
#include "includes.h"

// Register /dev/mouse (char major 13 minor 1). Exclusive open: only one
// reader at a time. Reads return whole mouse_event_t records; partial
// reads are truncated to a multiple of sizeof(mouse_event_t).
void mouse_dev_init(void);

// IRQ12 handler: drains aux bytes from the 8042 and feeds the packet
// assembler. Lives here (not in irq_handler.c) because it touches the
// mouse_dev internals.
void MouseHandler(void);
