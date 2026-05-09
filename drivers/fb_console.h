#pragma once
#include "includes.h"

// Initialise the framebuffer console. Reads fb_info, validates the embedded
// PSF2 font, computes screen dimensions in glyphs, clears the FB, and
// registers itself as the active screen console. No-op if no framebuffer
// was captured by Limine, or if the font header is malformed.
void fb_console_init(void);
