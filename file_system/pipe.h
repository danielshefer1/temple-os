#pragma once

#include "vfs_types.h"
#include "lock_types.h"

#define PIPE_BUF_SIZE   4096
#define PIPE_SIDE_READ  0
#define PIPE_SIDE_WRITE 1

// In-kernel pipe ring. Used both by anonymous pipe(2) pairs and by FIFOs
// (lazily attached to inode->pipe). Reader/writer counts track how many
// open file_t's reference each side; when both drop to zero the pipe is
// freed and (if FIFO) inode->pipe is cleared.
typedef struct pipe_t {
    spinlock_t lock;
    char*      buf;      // PIPE_BUF_SIZE bytes; allocated separately so
                         // sizeof(pipe_t) fits in a single slab object
    uint64_t   head;     // monotonic; index = head % PIPE_BUF_SIZE
    uint64_t   tail;     // monotonic
    uint32_t   readers;
    uint32_t   writers;
    // Sticky "a peer has existed at least once" flags. fifo_open's
    // wait-for-peer loop must re-check against these instead of the live
    // counter, otherwise a peer that opens-and-closes between schedule()
    // and our resume puts us back to sleep forever.
    bool       ever_readers;
    bool       ever_writers;

    struct task_t* read_waiter;       // single-link queue via task_t.next/prev
    struct task_t* read_waiter_tail;
    struct task_t* write_waiter;
    struct task_t* write_waiter_tail;

    inode_t* inode;       // non-NULL for FIFOs (so close can clear inode->pipe)
} pipe_t;

// One per file_t end. Stored in file_t.private_data; tells pipe_fops which
// side this descriptor represents.
typedef struct pipe_end_t {
    pipe_t* pipe;
    int     side;
} pipe_end_t;

extern struct file_ops_t pipe_fops;

// Anonymous pipe: allocate a pipe_t plus two file_t's (one per side) with
// pipe_fops installed. ref_count = 1 on each. Caller is responsible for
// fd_alloc'ing them. Returns 0 on success, -ENOMEM on allocation failure.
int64_t pipe_create_pair(file_t** rf_out, file_t** wf_out);

// FIFO open: lazily attach a pipe_t to in->pipe (allocating if needed),
// install pipe_fops on f, set f->private_data to a pipe_end_t describing
// which side based on flags (O_RDONLY -> read; O_WRONLY -> write; O_RDWR
// -> read, since we don't model full-duplex FIFOs).
int64_t fifo_open(inode_t* in, file_t* f, uint32_t flags);
