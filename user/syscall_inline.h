#pragma once

// Syscall ABI for this OS:
//   rax = number, rbx = arg1, r10 = arg2 (because rcx is clobbered by SYSCALL),
//   rdx = arg3, rsi = arg4, rdi = arg5, r8 = arg6, r9 = arg7.
// Returns in rax.

#define FWRITE_SYSCALL    10
#define EXIT_SYSCALL       1
#define FORK_SYSCALL      24
#define KILL_SYSCALL      25
#define SIGNAL_SYSCALL    26
#define SIGRETURN_SYSCALL 27
#define GETPID_SYSCALL    28
#define WAITPID_SYSCALL   29
#define STDOUT_FILENO      1
#define SIGINT             2

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
