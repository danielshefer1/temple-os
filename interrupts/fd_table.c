#include "fd_table.h"
#include "defintions.h"
#include "cpu_local.h"
#include "scheduler.h"

// Each task carries its own fd_entry_t fds[FD_MAX] array (see task_types.h).
// These accessors operate on the currently running task's array, so the
// callers in vfs_syscalls.c stay unchanged — they don't need to know which
// task they're servicing.

static inline fd_entry_t* current_fds(void) {
    return this_cpu()->current->fds;
}

static int64_t valid_fd(int64_t fd) {
    return (fd >= 0 && fd < FD_MAX) ? 0 : -EBADF;
}

int64_t fd_alloc(file_t* f) {
    if (f == NULL) return -EINVAL;
    fd_entry_t* fds = current_fds();
    // fds 0/1/2 are reserved for stdin/stdout/stderr; the syscall layer handles
    // them directly without touching this table.
    for (int64_t i = STDERR_FILENO + 1; i < FD_MAX; i++) {
        if (fds[i].file == NULL) {
            fds[i].file = f;
            fds[i].flags = 0;
            return i;
        }
    }
    return -EMFILE;
}

file_t* fd_lookup(int64_t fd) {
    if (valid_fd(fd) < 0) return NULL;
    return current_fds()[fd].file;
}

file_t* fd_release(int64_t fd) {
    if (valid_fd(fd) < 0) return NULL;
    fd_entry_t* fds = current_fds();
    file_t* f = fds[fd].file;
    if (f == NULL) return NULL;
    fds[fd].file = NULL;
    fds[fd].flags = 0;
    return f;
}
