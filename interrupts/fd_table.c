#include "fd_table.h"
#include "defintions.h"

typedef struct fd_entry_t {
    file_t*  file;
    uint32_t flags;
} fd_entry_t;

static fd_entry_t fd_table[FD_MAX];

static int64_t valid_fd(int64_t fd) {
    return (fd >= 0 && fd < FD_MAX) ? 0 : -EBADF;
}

int64_t fd_alloc(file_t* f) {
    if (f == NULL) return -EINVAL;
    // fds 0/1/2 are reserved for stdin/stdout/stderr; the syscall layer handles
    // them directly without touching this table.
    for (int64_t i = STDERR_FILENO + 1; i < FD_MAX; i++) {
        if (fd_table[i].file == NULL) {
            fd_table[i].file = f;
            fd_table[i].flags = 0;
            return i;
        }
    }
    return -EMFILE;
}

file_t* fd_lookup(int64_t fd) {
    if (valid_fd(fd) < 0) return NULL;
    return fd_table[fd].file;
}

file_t* fd_release(int64_t fd) {
    if (valid_fd(fd) < 0) return NULL;
    file_t* f = fd_table[fd].file;
    if (f == NULL) return NULL;
    fd_table[fd].file = NULL;
    fd_table[fd].flags = 0;
    return f;
}
