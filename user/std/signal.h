#pragma once

// Signal numbers (must match kernel multi/signal.h) and the user-side
// trampoline that returns control to the kernel after a SA-style handler
// returns. The kernel pushes the saved frame onto the user stack, then
// arranges for the handler to RET into sig_restorer; sig_restorer issues
// SIGRETURN so the kernel can pop the frame and resume.

#define SIGINT             2
#define SIGALRM           14
#define SIGTERM           15
#define SIGCHLD           17
#define SIGWINCH          28

// Naked: no prologue/epilogue, no GPR clobbers — the kernel restores the
// pre-signal register state from the saved frame on the user stack.
__attribute__((naked, used)) static void sig_restorer(void) {
    asm volatile(
        "mov $27, %rax\n"   // SIGRETURN_SYSCALL
        "syscall\n"
    );
}
