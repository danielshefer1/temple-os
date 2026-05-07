#include "waitpid.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "extern.h"
#include "defintions.h"

int64_t do_waitpid(int64_t target_pid, uint64_t* user_status) {
    // Validate the user pointer up front. Writing through it after we've
    // claimed the zombie would leak the child's task_t if the write faulted.
    if (user_status != NULL &&
        (uint64_t)user_status >= 0xFFFF800000000000ULL) {
        return -EINVAL;
    }

    task_t* self = this_cpu()->current;
    uint64_t want_pid = (target_pid > 0) ? (uint64_t)target_pid : 0;

    while (1) {
        // First: try to claim a zombie that already matches.
        task_t* z = zombie_list_take(self, want_pid);
        if (z != NULL) {
            uint64_t code = z->exit_code;
            uint64_t pid  = z->pid;
            free_dead_task(z);
            if (user_status) *user_status = code;
            return (int64_t)pid;
        }

        // No matching zombie. Confirm we have something worth waiting on.
        if (!task_has_children(self, want_pid)) {
            return -ECHILD;
        }

        // Park ourselves until a child exits. task_exit (in scheduler.c)
        // wakes a parent whose wait_target matches the exiting child's
        // pid (or whose wait_target is 0 = any child).
        CliHelper();
        self->wait_target = want_pid;
        self->state = TASK_STATE_BLOCKED;
        schedule();
        // Resumed: loop and re-scan. wait_target stays set; harmless.
    }
}
