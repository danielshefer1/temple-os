#pragma once
#include "includes.h"

// Framebuffer info captured from Limine before we leave its page tables.
// fb_phys is the physical FB base; fb_virt is the kernel-space mapping
// established in start(). When fb_phys == 0, no framebuffer is available.
typedef struct {
    uint64_t fb_phys;
    uint64_t fb_virt;
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint64_t size;
    uint16_t bpp;
    uint8_t  red_shift, red_size;
    uint8_t  green_shift, green_size;
    uint8_t  blue_shift, blue_size;
} fb_info_t;
