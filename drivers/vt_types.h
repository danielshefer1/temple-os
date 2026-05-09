#pragma once
#include "includes.h"
#include "lock_types.h"
#include "vt_defs.h"

typedef struct {
    char    ch;
    uint8_t fg;     // CGA index 0..15
    uint8_t bg;     // CGA index 0..15
} vt_cell_t;

typedef enum { VT_GROUND, VT_ESC, VT_CSI } vt_pstate_t;

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
