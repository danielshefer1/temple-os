#pragma once
#include "includes.h"
#include "lock_types.h"

#define NUM_VTS 6

typedef struct {
    char    ch;
    uint8_t fg;     // CGA index 0..15
    uint8_t bg;     // CGA index 0..15
} vt_cell_t;

typedef enum { VT_GROUND, VT_ESC, VT_CSI } vt_pstate_t;

#define VT_MAX_PARAMS 8

typedef struct vt {
    spinlock_t  lock;

    // Geometry (in cells). Set at init from fb_info / glyph dims.
    uint64_t    cols;
    uint64_t    rows;

    // Backbuffer: cols * rows cells. Source of truth for the VT's screen.
    // Active VT also has its mutations blitted to the framebuffer.
    vt_cell_t*  cells;

    // Cursor (in cells; 0-based).
    uint64_t    cur_row;
    uint64_t    cur_col;

    // Current SGR state, applied to glyphs as they're written.
    uint8_t     cur_fg;
    uint8_t     cur_bg;
    bool        cur_bold;

    // Parser state for ESC/CSI sequences targeted at this VT.
    vt_pstate_t pstate;
    uint32_t    params[VT_MAX_PARAMS];
    uint32_t    n_params;
    bool        has_param;
} vt_t;

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
