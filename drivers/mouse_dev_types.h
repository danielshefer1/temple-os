#pragma once
#include "includes.h"

// One decoded PS/2 mouse event. Userspace reads the /dev/mouse stream as a
// sequence of these. We normalize Y so that +y points downward (PS/2 wire
// format has +y = up) and zero-extend the button bits to a byte for
// alignment.
typedef struct mouse_event_t {
    int16_t dx;
    int16_t dy;
    uint8_t buttons;   // bit 0 = left, bit 1 = right, bit 2 = middle
    uint8_t _pad[3];
} mouse_event_t;
