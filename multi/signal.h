#pragma once

#include "includes.h"
#include "types.h"

// Mark `signo` as pending on `t` and, if `t` is BLOCKED, wake it so the
// signal can be delivered on its next return-to-userspace.
// Bits are atomic-or'd so signals from different CPUs don't lose each other.
void signal_send(task_t* t, int signo);

// Install a handler for the calling task. `handler` is SIG_DFL, SIG_IGN, or
// a user-space function pointer; `restorer` is a small user-space trampoline
// that calls SYS_SIGRETURN once the handler returns.
// Returns 0 on success, -EINVAL for SIGKILL/SIGSTOP or out-of-range signo.
int64_t signal_install(int signo, void* handler, void* restorer);

// Called at the tail of the syscall handler, after frame->rax has been set.
// If a signal is pending and the calling task has a non-default handler,
// rewrites the frame so SYSRET lands in the user handler with the original
// state preserved on the user stack for SIGRETURN to pop later.
void signal_deliver_on_return(interrupt_frame_t* frame);

// SYS_SIGRETURN handler: copies the saved interrupt_frame_t back from the
// user stack into `frame`. Returns the saved rax so the syscall handler's
// trailing assignment doesn't clobber it.
int64_t signal_sigreturn(interrupt_frame_t* frame);
