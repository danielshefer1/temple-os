#pragma once

// open() flags. Must match the bit layout the kernel decodes in
// interrupts/syscall_handler.c::OpenHandler / file_system/vfs_path_ops.c.

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002
#define O_CREAT  0x0040
#define O_TRUNC  0x0200
#define O_APPEND 0x0400
