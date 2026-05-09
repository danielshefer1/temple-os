#pragma once

// Syscall ABI for this OS:
//   rax = number, rbx = arg1, r10 = arg2 (because rcx is clobbered by SYSCALL),
//   rdx = arg3, rsi = arg4, rdi = arg5, r8 = arg6, r9 = arg7.
// Returns in rax.

#define FWRITE_SYSCALL    10
#define EXIT_SYSCALL       1
#define MMAP_SYSCALL       5
#define MUNMAP_SYSCALL     6
#define OPEN_SYSCALL       7
#define CLOSE_SYSCALL      8
#define FORK_SYSCALL      24
#define KILL_SYSCALL      25
#define SIGNAL_SYSCALL    26
#define SIGRETURN_SYSCALL 27
#define GETPID_SYSCALL    28
#define WAITPID_SYSCALL   29
#define MKNOD_SYSCALL     30
#define SLEEP_SYSCALL     31

// Inode types — must match VFS_TYPE_* in file_system/vfs_defs.h.
#define S_IFCHR_T  0x04
#define S_IFBLK_T  0x05

// dev_id encoding (Linux old form): low byte = minor, next byte = major.
#define UMKDEV(maj, min) ((((unsigned)(maj) & 0xFFu) << 8) | ((unsigned)(min) & 0xFFu))
#define STDOUT_FILENO      1
#define SIGINT             2

#define O_RDONLY 0x0000
#define O_WRONLY 0x0001
#define O_RDWR   0x0002

typedef long          ssize_t_;
typedef unsigned long size_t_;

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

// Create a device node at `path` with the given inode type (S_IFCHR_T /
// S_IFBLK_T), permissions, and packed dev_id (UMKDEV(major, minor)). The
// kernel resolves type+dev_id together; pass 0 for a regular non-device
// file (currently rejected — use sys_open(O_CREAT) instead).
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

static inline long sys_read_for_test_(long fd, void* buf, unsigned long size) {
    long ret;
    register void* r10_ asm("r10") = buf;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)9 /* FREAD_SYSCALL */), "b"(fd), "r"(r10_), "d"(size)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_lseek_for_test_(long fd, long offset, long whence) {
    long ret;
    register long r10_ asm("r10") = offset;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)11 /* LSEEK_SYSCALL */), "b"(fd), "r"(r10_), "d"(whence)
        : "rcx", "r11", "memory");
    return ret;
}

static inline long sys_unlink_for_test_(const char* path) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)13 /* UNLINK_SYSCALL */), "b"(path)
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

static inline long sys_fork(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)FORK_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

// ABI reminder: rcx is clobbered by SYSCALL; the kernel reads arg2 from r10
// and patches it back into frame->rcx. So inline asm must place arg2 in r10.

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

static inline long sys_sleep(unsigned long ms) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)SLEEP_SYSCALL), "b"(ms)
        : "rcx", "r11", "memory");
    return ret;
}

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

static inline long sys_getpid(void) {
    long ret;
    asm volatile(
        "syscall"
        : "=a"(ret)
        : "a"((long)GETPID_SYSCALL)
        : "rcx", "r11", "memory");
    return ret;
}

// Trampoline that returns control to the kernel after a signal handler RETs.
// Naked: no prologue/epilogue, no GPR clobbers — the kernel will restore the
// pre-signal register state from the saved frame on the user stack.
__attribute__((naked, used)) static void sig_restorer(void) {
    asm volatile(
        "mov $27, %rax\n"   // SIGRETURN_SYSCALL
        "syscall\n"
    );
}

static inline void sys_exit(long code) {
    asm volatile(
        "syscall"
        :
        : "a"((long)EXIT_SYSCALL), "b"(code)
        : "rcx", "r11", "memory");
    __builtin_unreachable();
}
