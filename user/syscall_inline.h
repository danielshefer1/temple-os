#pragma once

// Syscall ABI for this OS:
//   rax = number, rbx = arg1, r10 = arg2 (because rcx is clobbered by SYSCALL),
//   rdx = arg3, rsi = arg4, rdi = arg5, r8 = arg6, r9 = arg7.
// Returns in rax.

#define FWRITE_SYSCALL 10
#define EXIT_SYSCALL    1
#define STDOUT_FILENO   1

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

static inline void sys_exit(void) {
    asm volatile(
        "syscall"
        :
        : "a"((long)EXIT_SYSCALL)
        : "rcx", "r11", "memory");
    __builtin_unreachable();
}
