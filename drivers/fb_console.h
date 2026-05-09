#pragma once
#include "includes.h"
#include "vt.h"

// Initialise the FB blitter: validates fb_info + the embedded PSF2 font,
// computes glyph_w/h and how many cells fit on screen, and clears the FB.
// Returns the screen geometry in cells via *cols and *rows so vt_init_all
// can size each VT's cell backbuffer to match. No-op (returns false and
// leaves *cols/*rows zero) if no framebuffer is available.
bool fb_console_init(uint64_t* cols, uint64_t* rows);

// True if init succeeded (i.e. fb_blit_* are safe to call).
bool fb_console_ready(void);

// Pixel-level primitives. Caller is responsible for serialising calls
// (the per-VT lock in vt_t serves this role today).
void fb_blit_cell  (uint64_t row, uint64_t col, vt_cell_t cell);
void fb_clear_all  (uint8_t bg);
void fb_scroll_up  (uint64_t cell_rows, uint64_t cell_cols, uint8_t bg);

// Re-blit the entire backbuffer of `cells[rows*cols]` onto the FB. Used
// when switching active VTs.
void fb_redraw_cells(const vt_cell_t* cells, uint64_t rows, uint64_t cols);
