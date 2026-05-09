#include "signal.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "string.h"
#include "defintions.h"
#include "memory.h"
#include "global.h"
#include "extern.h"

// ---------------------------------------------------------------------------
// State helpers
// ---------------------------------------------------------------------------

static inline int signo_valid(int s) {
    return s > 0 && s < (int)NSIG;
}

void signal_send(task_t* t, int signo) {
    if (!t || !signo_valid(signo)) return;
    __atomic_or_fetch(&t->pending_signals, 1ULL << (signo - 1), __ATOMIC_RELAXED);

    // If the target was parked (e.g. waiting on a mutex), bring it back to
    // ready so it can pick up the signal. The corresponding wakeup paths
    // (mutex_unlock etc.) are unaware of signals; we duplicate the enqueue
    // here. If it was already READY/RUNNING/ZOMBIE we leave the state alone.
    if (t->state == TASK_STATE_BLOCKED) {
        t->state = TASK_STATE_READY;
        rq_enqueue_external(t);
    }
}

// Walk every CPU's current task and run-queue, signalling any task whose
// pgid matches. We skip the per-CPU sleep queue (sleeping tasks live on a
// run-queue too once the timer fires; signal_send re-enqueues BLOCKED tasks
// to READY so they pick up the signal on next dispatch). Zombies are not
// signalled — they have no userspace to deliver into.
void signal_send_pgrp(uint64_t pgrp, int signo) {
    if (pgrp == 0 || !signo_valid(signo)) return;
    uint64_t online = cpus_active ? cpus_active : 1;
    for (uint64_t i = 0; i < online; i++) {
        cpu_local_t* cpu = &cpu_locals[i];
        task_t* cur = cpu->current;
        if (cur && cur->pgid == pgrp && cur->state != TASK_STATE_ZOMBIE) {
            signal_send(cur, signo);
        }
        run_queue_t* rq = &cpu->rq;
        spin_lock(&rq->lock);
        for (task_t* t = rq->head; t; t = t->next) {
            if (t->pgid == pgrp && t->state != TASK_STATE_ZOMBIE) {
                signal_send(t, signo);
            }
        }
        spin_unlock(&rq->lock);
    }
}

int64_t signal_install(int signo, void* handler, void* restorer) {
    if (!signo_valid(signo)) return -EINVAL;
    if (signo == SIGKILL || signo == SIGSTOP) return -EINVAL;
    task_t* t = this_cpu()->current;
    t->signal_actions[signo].handler  = handler;
    t->signal_actions[signo].restorer = restorer;
    return 0;
}

// ---------------------------------------------------------------------------
// Default actions
// ---------------------------------------------------------------------------

static int default_terminates(int signo) {
    switch (signo) {
        case SIGCHLD:
        case SIGCONT:
            return 0;
        default:
            return 1;
    }
}

// ---------------------------------------------------------------------------
// Delivery: rewrite the kernel's saved frame to enter the user handler
// ---------------------------------------------------------------------------
//
// User-stack layout we build (low <- high addresses):
//
//   [restorer ret addr]    <- new userrsp; handler RETs into restorer
//   [saved interrupt_frame_t]
//   ...
//
// The restorer trampoline (provided by user code via signal_install) is
// expected to do nothing but `mov rax, SIGRETURN_SYSCALL; syscall`. After
// the handler RETs, user rsp points at the saved frame; the syscall then
// enters signal_sigreturn, which copies it back into the kernel's frame.

void signal_deliver_on_return(interrupt_frame_t* frame) {
    // Don't deliver if returning to ring 0 (shouldn't happen — kernel-mode
    // syscalls are blocked at the top of syscall_handler — but defensive).
    if (frame->cs != 0x23) return;

    task_t* t = this_cpu()->current;
    uint64_t pending = __atomic_load_n(&t->pending_signals, __ATOMIC_RELAXED);
    if (!pending) return;

    // Pick lowest pending signal.
    int signo = __builtin_ctzll(pending) + 1;
    __atomic_and_fetch(&t->pending_signals, ~(1ULL << (signo - 1)), __ATOMIC_RELAXED);

    // POSIX-ish: tasks killed by an unhandled signal exit with code 128+signo.
    uint64_t sig_exit_code = 128 + (uint64_t)signo;

    // SIGKILL is uncatchable.
    if (signo == SIGKILL) {
        task_exit(sig_exit_code);
        return;
    }

    void* handler  = t->signal_actions[signo].handler;
    void* restorer = t->signal_actions[signo].restorer;

    if (handler == SIG_IGN) return;
    if (handler == SIG_DFL) {
        if (default_terminates(signo)) task_exit(sig_exit_code);
        return;
    }
    if (!restorer) {
        // Misconfigured handler — can't reliably return to user state. Kill.
        task_exit(sig_exit_code);
        return;
    }

    // Build the user-side return frame. We're still on the kernel stack;
    // the user's CR3 is loaded so writes through user VAs land in the
    // user's address space.
    uint64_t user_rsp = frame->userrsp;

    user_rsp -= sizeof(interrupt_frame_t);
    user_rsp &= ~0xFULL;                       // 16-byte alignment
    memcpy((void*)user_rsp, frame, sizeof(*frame));

    user_rsp -= 8;
    *(uint64_t*)user_rsp = (uint64_t)restorer;

    // Redirect the SYSRET path into the user handler with rdi=signo.
    frame->rip     = (uint64_t)handler;
    frame->userrsp = user_rsp;
    frame->rdi     = (uint64_t)signo;
    // RFLAGS/CS/SS unchanged.
}

// ---------------------------------------------------------------------------
// SIGRETURN: restore from the saved frame
// ---------------------------------------------------------------------------

int64_t signal_sigreturn(interrupt_frame_t* frame) {
    // After the handler RETs, the restorer's `syscall` lands here with
    // user rsp pointing at the saved interrupt_frame_t.
    uint64_t saved_va = frame->userrsp;
    if (saved_va >= 0xFFFF800000000000ULL) return -EINVAL;

    interrupt_frame_t saved;
    memcpy(&saved, (void*)saved_va, sizeof(saved));

    // Don't let user code escape ring 3 by lying about cs/ss/rflags.
    if (saved.cs != 0x23 || saved.ss != 0x1B) return -EINVAL;

    memcpy(frame, &saved, sizeof(*frame));

    // syscall_handler will do `frame->rax = ret` after we return; returning
    // saved.rax means the post-handler rax is the value rax had at the
    // time of the original interruption (e.g. the syscall result).
    return (int64_t)saved.rax;
}
