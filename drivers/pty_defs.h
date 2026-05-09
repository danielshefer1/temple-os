#pragma once

#define PTY_BUF_SIZE      4096
#define PTY_MAX_PAIRS     8

// Linux device-major conventions.
#define PTY_PTMX_MAJOR    5
#define PTY_PTMX_MINOR    2
#define PTY_SLAVE_MAJOR 136     // /dev/pts/N (UNIX98)

// Pty-specific ioctls. Numeric values follow the existing TTY_IOCTL_* style
// (drivers/tty_types.h) — they share the IOCTL_SYSCALL number-space.
#define TIOCGPTN          0x110   // get pts index from master (arg = uint32_t*)
#define TIOCSPTLCK        0x111   // (un)lock slave open (arg = int*; 0 unlocks)
#define TIOCSCTTY         0x112   // make this fd the calling task's controlling tty
#define TIOCSWINSZ        0x113   // set window size (arg = winsize_t*)
