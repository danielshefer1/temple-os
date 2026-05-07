#include "syscall_inline.h"

static const char start_m[]  = "starting\n";
static const char child_m[]  = "child running\n";
static const char wait_m[]   = "parent reaped child, status=";
static const char nl_m[]     = "\n";
static const char loop_done[] = "loop done, all children reaped\n";

__attribute__((used)) const char* volatile msg_ptr = start_m;

static unsigned long my_strlen(const char* s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

// Tiny base-10 itoa into a fixed buffer. Returns length written.
static unsigned long itoa10(unsigned long v, char* buf) {
    char tmp[24];
    unsigned long n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (char)(v % 10); v /= 10; }
    for (unsigned long i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    buf[n] = '\n';
    return n + 1;
}

void _start(void) {
    sys_write(STDOUT_FILENO, msg_ptr, my_strlen(msg_ptr));

    // Loop: fork + waitpid 5 times. Verifies waitpid blocks and unblocks
    // correctly, propagates exit codes, and (crucially) doesn't leak
    // user-AS pages — without the drain_pending_reap fix this would burn
    // ~16 KB+ of user pages per iteration.
    for (long iter = 1; iter <= 5; iter++) {
        long pid = sys_fork();
        if (pid == 0) {
            sys_write(STDOUT_FILENO, child_m, my_strlen(child_m));
            sys_exit(iter * 10);   // distinctive per-child exit code
        }

        unsigned long status = 0;
        long reaped = sys_waitpid(pid, &status);
        (void)reaped;

        sys_write(STDOUT_FILENO, wait_m, my_strlen(wait_m));
        char buf[32];
        unsigned long n = itoa10(status, buf);
        sys_write(STDOUT_FILENO, buf, n);
    }

    sys_write(STDOUT_FILENO, loop_done, my_strlen(loop_done));
    sys_exit(0);
}
