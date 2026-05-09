#include "syscall_inline.h"

// PID 1. Forks a periodic filesystem syncer, then supervises /bin/term,
// respawning it whenever it exits. Reaps any other children opportunistically
// (orphan reparenting to PID 1 is not implemented in the kernel yet, so in
// practice these are just init's own direct children).

static void syncer_loop(void) {
    for (;;) {
        sys_sleep(5000);
        sys_sync();
    }
}

void _start(void) {
    long syncer_pid = sys_fork();
    if (syncer_pid == 0) {
        syncer_loop();
        sys_exit(0);
    }

    for (;;) {
        char* const term_argv[] = { "term", 0 };
        long term_pid = sys_spawn("/bin/term", term_argv, 0);
        if (term_pid < 0) {
            sys_sleep(1000);
            continue;
        }
        for (;;) {
            unsigned long st = 0;
            long r = sys_waitpid(-1, &st);
            if (r == term_pid) break;
            if (r < 0) { sys_sleep(1000); break; }
        }
    }
}
