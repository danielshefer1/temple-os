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
        // Fast path: claim a zombie that already matches.
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

        // Park-and-recheck under zombie_list_lock. Required: a child that
        // exits between the fast-path scan above and our BLOCKED transition
        // could otherwise read state == RUNNING in task_exit, skip the
        // wake, and leave us parked with no future event to revive us.
        // Holding the same lock task_exit holds for the wake closes the
        // window: either the recheck finds the new zombie, or task_exit
        // observes BLOCKED and wakes us.
        CliHelper();
        spin_lock(&zombie_list_lock);
        z = zombie_list_take_locked(self, want_pid);
        if (z != NULL) {
            spin_unlock(&zombie_list_lock);
            StiHelper();
            uint64_t code = z->exit_code;
            uint64_t pid  = z->pid;
            free_dead_task(z);
            if (user_status) *user_status = code;
            return (int64_t)pid;
        }
        self->wait_target = want_pid;
        self->state = TASK_STATE_BLOCKED;
        spin_unlock(&zombie_list_lock);

        // IRQs intentionally stay off across schedule() — schedule()
        // captures the caller's IF and restores it on resume, matching
        // pipe_read / pipe_write.
        schedule();
        // Resumed: loop and re-scan. wait_target stays set; harmless.
    }
}
