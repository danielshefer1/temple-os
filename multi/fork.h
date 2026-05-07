#pragma once

#include "includes.h"
#include "types.h"

// POSIX fork: clone the calling user task. The child gets its own PID, its
// own deep-copied user address space, the same kstack layout shaped for a
// post-syscall SYSRET, and is enqueued on its home CPU's run queue.
//
// Returns the child's PID (parent's view) on success or -errno on failure.
// The child reaches userspace with rax=0, all other GPRs preserved from the
// parent, and resumes at the user instruction immediately after `syscall`.
int64_t do_fork(interrupt_frame_t* parent_frame);
