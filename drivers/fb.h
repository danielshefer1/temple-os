#pragma once
#include "includes.h"

// Map the framebuffer captured by limine_entry into FB_VIRTUAL and store the
// mapped pointer in fb_info.fb_virt. No-op if no FB was captured.
void fb_map(void);

// Solid-color fill, used as an M1 sanity check. RGB is encoded into the
// device's pixel format using fb_info masks. Safe to call only after fb_map().
void fb_clear(uint32_t rgb);

// Pack a 24-bit RGB triple into the FB's native pixel layout.
uint32_t fb_pack(uint8_t r, uint8_t g, uint8_t b);

// M1 sanity pattern (red field with a green vertical band and a blue
// horizontal band crossing in the middle).
void fb_test_pattern(void);
