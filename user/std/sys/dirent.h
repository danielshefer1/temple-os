#pragma once

// Linux-compatible getdents64 ABI. The kernel packs entries into the user
// buffer end-to-end; each entry's d_reclen tells you how far to skip to
// reach the next one. Iteration ends when getdents returns 0.

#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK     10
#define DT_SOCK    12

struct linux_dirent64 {
    unsigned long  d_ino;
    long           d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};
