#pragma once

#include "std/sys/types.h"
#include "std/sys/stat.h"

// stdtemple syscall layer.
//
// Syscall ABI for this OS:
//   rax = number, rbx = arg1, r10 = arg2 (because rcx is clobbered by
//   the SYSCALL instruction), rdx = arg3, rsi = arg4, rdi = arg5,
//   r8 = arg6, r9 = arg7.
// Returns in rax (negative for -errno on failure).

// ---- syscall numbers ---------------------------------------------------

#define EXIT_SYSCALL       1
#define MMAP_SYSCALL       5
#define MUNMAP_SYSCALL     6
#define OPEN_SYSCALL       7
#define CLOSE_SYSCALL      8
#define FREAD_SYSCALL      9
#define FWRITE_SYSCALL    10
#define LSEEK_SYSCALL     11
#define TRUNCATE_SYSCALL  12
#define UNLINK_SYSCALL    13
#define MKDIR_SYSCALL     14
#define RMDIR_SYSCALL     15
#define RENAME_SYSCALL    16
#define SYMLINK_SYSCALL   17
#define READLINK_SYSCALL  18
#define STAT_SYSCALL      19
#define SYNC_SYSCALL      20
#define IOCTL_SYSCALL     21
#define EXEC_SYSCALL      22
#define SPAWN_SYSCALL     23
#define FORK_SYSCALL      24
#define KILL_SYSCALL      25
#define SIGNAL_SYSCALL    26
#define SIGRETURN_SYSCALL 27
#define GETPID_SYSCALL    28
#define WAITPID_SYSCALL   29
#define MKNOD_SYSCALL     30
#define SLEEP_SYSCALL     31
#define SETPGID_SYSCALL   32
#define GETPGID_SYSCALL   33
#define PIPE_SYSCALL      34
#define DUP_SYSCALL       35
#define DUP2_SYSCALL      36
#define CHDIR_SYSCALL     37
#define GETCWD_SYSCALL    38
#define GETDENTS_SYSCALL  39
#define MMAP_FILE_SYSCALL 40
#define SETSID_SYSCALL    41
#define SHUTDOWN_SYSCALL  42

#define STDIN_FILENO       0
#define STDOUT_FILENO      1
#define STDERR_FILENO      2

#define SEEK_SET           0
#define SEEK_CUR           1
#define SEEK_END           2

// ---- file I/O ----------------------------------------------------------

static inline long sys_write(long fd, const void* buf, unsigned long size) {
    long ret;
    register const void* r10_ asm("r10") = buf;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)FWRITE_SYSCALL), "b"(fd), "r"(r10_), "d"(size)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_read(long fd, void* buf, unsigned long size) {
    long ret;
    register void* r10_ asm("r10") = buf;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)FREAD_SYSCALL), "b"(fd), "r"(r10_), "d"(size)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_open(const char* path, long flags, long mode) {
    long ret;
    register long r10_ asm("r10") = flags;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)OPEN_SYSCALL), "b"(path), "r"(r10_), "d"(mode)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_close(long fd) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)CLOSE_SYSCALL), "b"(fd)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_lseek(long fd, long offset, long whence) {
    long ret;
    register long r10_ asm("r10") = offset;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)LSEEK_SYSCALL), "b"(fd), "r"(r10_), "d"(whence)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_truncate(const char* path, long length) {
    long ret;
    register long r10_ asm("r10") = length;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)TRUNCATE_SYSCALL), "b"(path), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

// ---- filesystem path ops -----------------------------------------------

static inline long sys_unlink(const char* path) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)UNLINK_SYSCALL), "b"(path)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_mkdir(const char* path, long perm) {
    long ret;
    register long r10_ asm("r10") = perm;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)MKDIR_SYSCALL), "b"(path), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_rmdir(const char* path) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)RMDIR_SYSCALL), "b"(path)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_rename(const char* oldpath, const char* newpath) {
    long ret;
    register const char* r10_ asm("r10") = newpath;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)RENAME_SYSCALL), "b"(oldpath), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_symlink(const char* target, const char* link) {
    long ret;
    register const char* r10_ asm("r10") = link;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SYMLINK_SYSCALL), "b"(target), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_readlink(const char* path, char* buf, unsigned long size) {
    long ret;
    register char* r10_ asm("r10") = buf;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)READLINK_SYSCALL), "b"(path), "r"(r10_), "d"(size)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_stat(const char* path, stat_t* out) {
    long ret;
    register stat_t* r10_ asm("r10") = out;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)STAT_SYSCALL), "b"(path), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_sync(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SYNC_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_chdir(const char* path) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)CHDIR_SYSCALL), "b"(path)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_getcwd(char* buf, unsigned long size) {
    long ret;
    register unsigned long r10_ asm("r10") = size;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)GETCWD_SYSCALL), "b"(buf), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_getdents(long fd, void* buf, unsigned long size) {
    long ret;
    register void* r10_ asm("r10") = buf;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)GETDENTS_SYSCALL), "b"(fd), "r"(r10_), "d"(size)
        : "rcx", "r11", "memory");
    return ret;
}

// ---- device nodes / ioctl ----------------------------------------------

// Create a device node at `path` with the given inode type (S_IFCHR /
// S_IFBLK / S_IFIFO), permissions, and packed dev_id (UMKDEV(major, minor)).
// Pass dev_id = 0 for FIFOs.
static inline long sys_mknod(const char* path, long type, long perm, long dev_id) {
    long ret;
    register long r10_ asm("r10") = type;
    register long arg4 asm("rsi") = dev_id;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)MKNOD_SYSCALL), "b"(path), "r"(r10_), "d"(perm), "r"(arg4)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_ioctl(long fd, long cmd, void* arg) {
    long ret;
    register long r10_ asm("r10") = cmd;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)IOCTL_SYSCALL), "b"(fd), "r"(r10_), "d"(arg)
        : "rcx", "r11", "memory");
    return ret;
}

// ---- memory ------------------------------------------------------------

static inline void* sys_mmap(unsigned long size) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)MMAP_SYSCALL), "b"(size)
        : "rcx", "r11", "memory");
    return (void*)ret;
}

static inline long sys_munmap(void* addr, unsigned long size) {
    long ret;
    register unsigned long r10_ asm("r10") = size;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)MUNMAP_SYSCALL), "b"(addr), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline void* sys_mmap_file(long fd, unsigned long size) {
    long ret;
    register unsigned long r10_ asm("r10") = size;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)MMAP_FILE_SYSCALL), "b"(fd), "r"(r10_)
        : "rcx", "r11", "memory");
    return (void*)ret;
}

// ---- process / signal --------------------------------------------------

static inline long sys_fork(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)FORK_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_exec(const char* path,
                            char* const argv[], char* const envp[]) {
    long ret;
    register char* const* r10_ asm("r10") = argv;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)EXEC_SYSCALL), "b"(path), "r"(r10_), "d"(envp)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_spawn(const char* path,
                             char* const argv[], char* const envp[]) {
    long ret;
    register char* const* r10_ asm("r10") = argv;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SPAWN_SYSCALL), "b"(path), "r"(r10_), "d"(envp)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_kill(long pid, long signo) {
    long ret;
    register long r10_ asm("r10") = signo;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)KILL_SYSCALL), "b"(pid), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_signal(long signo, void* handler, void* restorer) {
    long ret;
    register void* r10_ asm("r10") = handler;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SIGNAL_SYSCALL), "b"(signo), "r"(r10_), "d"(restorer)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_waitpid(long pid, unsigned long* status) {
    long ret;
    register unsigned long* r10_ asm("r10") = status;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)WAITPID_SYSCALL), "b"(pid), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_getpid(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)GETPID_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

// setpgid(pid, pgid). pid==0 means self; pgid==0 means "use the target's
// own pid", i.e. start a new pgrp with the target as leader. See
// SetpgidHandler in interrupts/syscall_handler.c for the kernel side.
static inline long sys_setpgid(long pid, long pgid) {
    long ret;
    register long r10_ asm("r10") = pgid;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SETPGID_SYSCALL), "b"(pid), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_setsid(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SETSID_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_sleep(unsigned long ms) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SLEEP_SYSCALL), "b"(ms)
        : "rcx", "r11", "memory");
    return ret;
}

// ---- pipes / fd management --------------------------------------------

static inline long sys_pipe(int fds[2]) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)PIPE_SYSCALL), "b"(fds)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_dup(long fd) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)DUP_SYSCALL), "b"(fd)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_dup2(long oldfd, long newfd) {
    long ret;
    register long r10_ asm("r10") = newfd;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)DUP2_SYSCALL), "b"(oldfd), "r"(r10_)
        : "rcx", "r11", "memory");
    return ret;
}

// ---- shutdown / exit --------------------------------------------------

static inline long sys_shutdown(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SHUTDOWN_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

static inline void sys_exit(long code) {
    asm volatile(
        "syscall"
        :
        : "a"((long)EXIT_SYSCALL), "b"(code)
        : "rcx", "r11", "memory");
    __builtin_unreachable();
}
