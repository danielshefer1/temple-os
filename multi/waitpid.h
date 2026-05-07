#pragma once

#include "includes.h"
#include "types.h"

// POSIX-ish waitpid. Blocks the caller until a matching child reaches the
// ZOMBIE state, reaps it, and returns its PID. Writes the child's exit
// code to *user_status if non-NULL. `target_pid > 0` waits for that
// specific child; `target_pid == -1` (or 0) waits for any child.
//
// Returns:
//   pid > 0  — child was reaped; *user_status holds its exit code.
//   -ECHILD  — caller has no children matching the request.
//   -EINVAL  — user_status is non-NULL but points into the kernel half.
int64_t do_waitpid(int64_t target_pid, uint64_t* user_status);
