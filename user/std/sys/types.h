#pragma once

// Userspace ABI typedefs and structs shared across stdtemple. Kept minimal
// since this is a freestanding (no-libc) build — every headers user is a
// kernel-shape program.

typedef long          ssize_t_;
typedef unsigned long size_t_;

// Framebuffer and tty geometry — passed as the third arg to ioctl() on
// /dev/fb (FBIOGET_VSCREENINFO) and on a pty (TIOCGWINSZ / TIOCSWINSZ).
typedef struct fb_var_info_t {
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    unsigned int bpp;
} fb_var_info_t;

typedef struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
} winsize_t;
