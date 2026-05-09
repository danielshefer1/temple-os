#pragma once
#include "includes.h"
#include "lock_types.h"
#include "vt_defs.h"
#include "vt_types.h"

extern vt_t  vts[NUM_VTS];
extern vt_t* active_vt;

// Initialise all VTs once fb_console primitives are ready (geometry + glyph
// size known). Allocates each VT's cell backbuffer via the kernel buddy.
void vt_init_all(void);

// Feed one byte to a VT's parser/renderer. If `vt == active_vt`, mutated
// cells are also blitted to the FB; otherwise the change only lives in the
// backbuffer and becomes visible on the next switch.
void vt_write_byte(vt_t* vt, char c);

// Switch the active VT. Triggers a full redraw of `vts[idx]`'s backbuffer
// onto the framebuffer. Out-of-range indices are silently ignored.
void vt_switch_to(uint64_t idx);

// Shorthand for the common path: write a byte to whichever VT is currently
// active. Safe to call before vt_init_all (no-op when no VT is up).
static inline void vt_write_active_(char c) {
    extern vt_t* active_vt;
    if (active_vt) vt_write_byte(active_vt, c);
}
