#pragma once
#include "includes.h"

// Register /dev/fb (char major 29 minor 0). After this:
//   ioctl(fd, FBIOGET_VSCREENINFO, &fb_var_info) -> width/height/pitch/bpp
//   mmap-via-MMAP_FILE_SYSCALL(fd, size) -> framebuffer pixel bytes
void fb_dev_init(void);

// fb_var_info layout exposed to userspace via FBIOGET_VSCREENINFO. Keeps it
// minimal — width/height/pitch/bpp are what a userspace term needs.
typedef struct fb_var_info_t {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
} fb_var_info_t;

#define FBIOGET_VSCREENINFO   0x120
