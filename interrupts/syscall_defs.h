#pragma once
#include "includes.h"

#define EXIT_SYSCALL 1
#define FLUSH_BUFFER_SYSCALL 4
#define MMAP_SYSCALL 5
#define MUNMAP_SYSCALL 6
#define OPEN_SYSCALL       7
#define CLOSE_SYSCALL      8
#define FREAD_SYSCALL      9
#define FWRITE_SYSCALL     10
#define LSEEK_SYSCALL      11
#define TRUNCATE_SYSCALL   12
#define UNLINK_SYSCALL     13
#define MKDIR_SYSCALL      14
#define RMDIR_SYSCALL      15
#define RENAME_SYSCALL     16
#define SYMLINK_SYSCALL    17
#define READLINK_SYSCALL   18
#define STAT_SYSCALL       19
#define SYNC_SYSCALL       20
#define IOCTL_SYSCALL      21
#define EXEC_SYSCALL       22
#define SPAWN_SYSCALL      23
#define FORK_SYSCALL       24
#define KILL_SYSCALL       25
#define SIGNAL_SYSCALL     26
#define SIGRETURN_SYSCALL  27
#define GETPID_SYSCALL     28
#define WAITPID_SYSCALL    29
#define MKNOD_SYSCALL      30
#define SLEEP_SYSCALL      31

#define FD_MAX             64
#define EXEC_PATH_MAX      256

#define STDIN_FILENO   0
#define STDOUT_FILENO  1
#define STDERR_FILENO  2
