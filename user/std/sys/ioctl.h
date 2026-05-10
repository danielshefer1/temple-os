#pragma once

// ioctl command numbers used by userspace. Must match the kernel-side
// definitions in drivers/pty_defs.h, drivers/fb_dev.h, drivers/tty.c.

#define TTY_IOCTL_SET_RAW    0x100
#define TTY_IOCTL_SET_COOKED 0x101
#define TIOCGWINSZ        0x105
#define TIOCSPGRP_U       0x103
#define TIOCGPTN          0x110
#define TIOCSPTLCK        0x111
#define TIOCSCTTY         0x112
#define TIOCSWINSZ        0x113
#define FBIOGET_VSCREENINFO 0x120
