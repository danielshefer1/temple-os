#include "pipe.h"
#include "vfs.h"
#include "string.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "signal.h"
#include "extern.h"
#include "defintions.h"

// Single shared stub inode for anonymous pipes. vfs_check_file requires a
// non-NULL inode whose type isn't VFS_TYPE_DIR; pipe_fops never dereferences
// f->inode beyond that check, so one global instance is fine. Same trick
// drivers/tty.c uses with tty_stub_inode.
static inode_t pipe_stub_inode;
static bool    pipe_stub_init = false;

// Serialises FIFO inode->pipe transitions. fifo_open's check-and-alloc and
// pipe_close's NULL-and-free are otherwise racy: two concurrent opens of a
// fresh FIFO can both observe in->pipe == NULL and allocate distinct pipes,
// then end up parked on disjoint waiter queues with no peer to wake them
// (a deadlock, not just a leak as an earlier comment suggested). Lock
// order: fifo_attach_lock → p->lock; both fifo_open and pipe_close take
// them in that order.
static spinlock_t fifo_attach_lock = {0};

static void pipe_free(pipe_t* p) {
    if (!p) return;
    if (p->buf) kfree(p->buf, PIPE_BUF_SIZE);
    kfree(p, sizeof(pipe_t));
}

static void ensure_stub_inode(void) {
    if (pipe_stub_init) return;
    memset(&pipe_stub_inode, 0, sizeof(pipe_stub_inode));
    pipe_stub_inode.type = VFS_TYPE_FIFO;
    pipe_stub_init = true;
}

// ---- ring helpers (assume p->lock held) ---------------------------------

static inline uint64_t ring_count(pipe_t* p) { return p->head - p->tail; }
static inline uint64_t ring_space(pipe_t* p) { return PIPE_BUF_SIZE - ring_count(p); }

static uint64_t ring_drain(pipe_t* p, char* dst, uint64_t size) {
    uint64_t n = 0;
    while (p->tail != p->head && n < size) {
        dst[n++] = p->buf[p->tail % PIPE_BUF_SIZE];
        p->tail++;
    }
    return n;
}

static uint64_t ring_fill(pipe_t* p, const char* src, uint64_t size) {
    uint64_t n = 0;
    while (ring_space(p) > 0 && n < size) {
        p->buf[p->head % PIPE_BUF_SIZE] = src[n++];
        p->head++;
    }
    return n;
}

// ---- waiter queues (assume p->lock held) --------------------------------

static void waiter_enqueue(task_t** head, task_t** tail, task_t* me) {
    me->next = NULL;
    me->prev = *tail;
    if (*tail) (*tail)->next = me;
    else       *head = me;
    *tail = me;
}

static task_t* waiter_pop(task_t** head, task_t** tail) {
    task_t* w = *head;
    if (!w) return NULL;
    *head = w->next;
    if (*head) (*head)->prev = NULL;
    else       *tail = NULL;
    w->next = w->prev = NULL;
    return w;
}

// Splice `me` out of (head, tail) if it's still on the queue. Used by a
// task that woke up via signal_send rather than via a peer's pop. Mirrors
// the equivalent block in drivers/tty.c::tty_read.
static void waiter_remove_self(task_t** head, task_t** tail, task_t* me) {
    if (!me->next && !me->prev && *head != me) return;
    if (me->prev) me->prev->next = me->next;
    else if (*head == me) *head = me->next;
    if (me->next) me->next->prev = me->prev;
    else if (*tail == me) *tail = me->prev;
    me->next = me->prev = NULL;
}

// Pop everyone off the queue, mark READY, return them in a chain via .next
// (with .prev cleared) so the caller can push them onto the run queue
// without holding our lock. Used by close() to wake all waiters at EOF.
static task_t* waiter_drain_all(task_t** head, task_t** tail) {
    task_t* chain = *head;
    *head = *tail = NULL;
    for (task_t* t = chain; t != NULL; t = t->next) {
        t->state = TASK_STATE_READY;
        t->prev = NULL;  // detach from queue's back-link
    }
    return chain;
}

// ---- file_ops -----------------------------------------------------------

static int64_t pipe_read(file_t* f, void* buf, uint64_t size) {
    if (size == 0) return 0;
    pipe_end_t* end = (pipe_end_t*)f->private_data;
    if (!end || end->side != PIPE_SIDE_READ) return -EBADF;
    pipe_t* p = end->pipe;
    char* dst = (char*)buf;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);

    while (1) {
        if (ring_count(p) > 0) {
            uint64_t n = ring_drain(p, dst, size);
            // Wake one writer that may have been blocked on full-ring.
            task_t* w = waiter_pop(&p->write_waiter, &p->write_waiter_tail);
            spin_unlock(&p->lock);
            if (w) { w->state = TASK_STATE_READY; rq_enqueue_external(w); }
            if (ie) StiHelper();
            return (int64_t)n;
        }
        if (p->writers == 0) {
            // No data and no writers — EOF.
            spin_unlock(&p->lock);
            if (ie) StiHelper();
            return 0;
        }
        // Park on the read-waiter queue and reschedule.
        task_t* me = this_cpu()->current;
        waiter_enqueue(&p->read_waiter, &p->read_waiter_tail, me);
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&p->lock);

        schedule();

        CliHelper();
        spin_lock(&p->lock);
        waiter_remove_self(&p->read_waiter, &p->read_waiter_tail, me);
    }
}

static int64_t pipe_write(file_t* f, const void* buf, uint64_t size) {
    if (size == 0) return 0;
    pipe_end_t* end = (pipe_end_t*)f->private_data;
    if (!end || end->side != PIPE_SIDE_WRITE) return -EBADF;
    pipe_t* p = end->pipe;
    const char* src = (const char*)buf;
    uint64_t total = 0;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);

    while (total < size) {
        if (p->readers == 0) {
            // Broken pipe: SIGPIPE (which by default kills the task) and -EPIPE.
            spin_unlock(&p->lock);
            signal_send(this_cpu()->current, SIGPIPE);
            if (ie) StiHelper();
            return (total > 0) ? (int64_t)total : -EPIPE;
        }
        if (ring_space(p) > 0) {
            uint64_t n = ring_fill(p, src + total, size - total);
            total += n;
            // Wake one reader (typical: there's at most one parked).
            task_t* r = waiter_pop(&p->read_waiter, &p->read_waiter_tail);
            spin_unlock(&p->lock);
            if (r) { r->state = TASK_STATE_READY; rq_enqueue_external(r); }
            if (total == size) {
                if (ie) StiHelper();
                return (int64_t)total;
            }
            CliHelper();
            spin_lock(&p->lock);
            continue;
        }
        // Full and there are still readers — block.
        task_t* me = this_cpu()->current;
        waiter_enqueue(&p->write_waiter, &p->write_waiter_tail, me);
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&p->lock);

        schedule();

        CliHelper();
        spin_lock(&p->lock);
        waiter_remove_self(&p->write_waiter, &p->write_waiter_tail, me);
    }

    spin_unlock(&p->lock);
    if (ie) StiHelper();
    return (int64_t)total;
}

static int64_t pipe_close(file_t* f) {
    pipe_end_t* end = (pipe_end_t*)f->private_data;
    if (!end) return 0;
    pipe_t* p = end->pipe;
    bool is_fifo = (p->inode != NULL);

    bool ie = check_interrupts();
    CliHelper();
    // For FIFOs, take fifo_attach_lock first so the inode->pipe clear and
    // pipe_free happen atomically w.r.t. a concurrent fifo_open's check.
    // Without this, fifo_open can read in->pipe after we unlock p->lock
    // but before we clear the inode pointer and free, then increment
    // counters on a pipe we're about to kfree. Lock order matches
    // fifo_open: fifo_attach_lock → p->lock.
    if (is_fifo) spin_lock(&fifo_attach_lock);
    spin_lock(&p->lock);

    if (end->side == PIPE_SIDE_READ) {
        if (p->readers > 0) p->readers--;
    } else {
        if (p->writers > 0) p->writers--;
    }

    // If our side just hit zero, the other side's blocked tasks need to
    // re-evaluate (readers will see EOF; writers will see -EPIPE). Drain
    // the *opposite* queue in full.
    task_t* wake_chain = NULL;
    if (end->side == PIPE_SIDE_READ && p->readers == 0) {
        wake_chain = waiter_drain_all(&p->write_waiter, &p->write_waiter_tail);
    }
    if (end->side == PIPE_SIDE_WRITE && p->writers == 0) {
        wake_chain = waiter_drain_all(&p->read_waiter, &p->read_waiter_tail);
    }

    bool free_pipe = (p->readers == 0 && p->writers == 0);
    inode_t* fifo_inode = free_pipe ? p->inode : NULL;
    if (fifo_inode) fifo_inode->pipe = NULL;

    spin_unlock(&p->lock);
    if (is_fifo) spin_unlock(&fifo_attach_lock);

    // Push woken tasks onto run queues, outside both locks.
    while (wake_chain) {
        task_t* nxt = wake_chain->next;
        wake_chain->next = NULL;
        rq_enqueue_external(wake_chain);
        wake_chain = nxt;
    }

    if (free_pipe) {
        pipe_free(p);
    }
    kfree(end, sizeof(pipe_end_t));
    f->private_data = NULL;

    if (ie) StiHelper();
    return 0;
}

file_ops_t pipe_fops = {
    .read     = pipe_read,
    .write    = pipe_write,
    .seek     = NULL,
    .truncate = NULL,
    .readdir  = NULL,
    .open     = NULL,
    .close    = pipe_close,
    .flush    = NULL,
    .ioctl    = NULL,
};

// ---- public --------------------------------------------------------------

static pipe_t* pipe_alloc(inode_t* fifo_inode) {
    pipe_t* p = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!p) return NULL;
    memset(p, 0, sizeof(*p));
    p->buf = (char*)kmalloc(PIPE_BUF_SIZE);
    if (!p->buf) { kfree(p, sizeof(pipe_t)); return NULL; }
    p->inode = fifo_inode;
    return p;
}

static int install_end(file_t* f, pipe_t* p, int side) {
    pipe_end_t* end = (pipe_end_t*)kmalloc(sizeof(pipe_end_t));
    if (!end) return -ENOMEM;
    end->pipe = p;
    end->side = side;

    ensure_stub_inode();
    f->inode = (p->inode != NULL) ? p->inode : &pipe_stub_inode;
    f->ops = &pipe_fops;
    f->position = 0;
    f->mode = 0;
    f->ref_count = 1;
    f->private_data = end;
    return 0;
}

int64_t pipe_create_pair(file_t** rf_out, file_t** wf_out) {
    if (!rf_out || !wf_out) return -EINVAL;

    pipe_t* p = pipe_alloc(NULL);
    if (!p) return -ENOMEM;
    p->readers = 1;
    p->writers = 1;

    file_t* rf = vfs_file_alloc();
    file_t* wf = vfs_file_alloc();
    if (!rf || !wf) goto fail;

    if (install_end(rf, p, PIPE_SIDE_READ) < 0) goto fail;
    rf->flags = O_RDONLY;
    if (install_end(wf, p, PIPE_SIDE_WRITE) < 0) {
        // Undo install_end(rf): free its end and clear so vfs_file_put
        // doesn't double-decrement readers via pipe_close.
        kfree(rf->private_data, sizeof(pipe_end_t));
        rf->private_data = NULL;
        rf->ops = NULL;
        goto fail;
    }
    wf->flags = O_WRONLY;

    *rf_out = rf;
    *wf_out = wf;
    return 0;

fail:
    if (rf) kfree(rf, sizeof(file_t));
    if (wf) kfree(wf, sizeof(file_t));
    pipe_free(p);
    return -ENOMEM;
}

int64_t fifo_open(inode_t* in, file_t* f, uint32_t flags) {
    if (!in || !f) return -EINVAL;

    int side;
    uint32_t acc = flags & O_ACCMODE;
    if (acc == O_WRONLY) side = PIPE_SIDE_WRITE;
    else                 side = PIPE_SIDE_READ;   // RDONLY and RDWR map to read

    // Lazily attach a pipe_t to the inode under fifo_attach_lock so two
    // concurrent opens of a fresh FIFO can't each allocate their own pipe
    // and end up parked on disjoint waiter queues. Also covers the close
    // race: pipe_close NULLs inode->pipe under this lock, and we bump
    // readers/writers before releasing it, so we can never observe a live
    // inode->pipe and have it freed out from under us.
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&fifo_attach_lock);
    if (in->pipe == NULL) {
        pipe_t* new_p = pipe_alloc(in);
        if (!new_p) {
            spin_unlock(&fifo_attach_lock);
            if (ie) StiHelper();
            return -ENOMEM;
        }
        in->pipe = new_p;
    }
    pipe_t* p = in->pipe;
    spin_lock(&p->lock);
    if (side == PIPE_SIDE_READ) { p->readers++; p->ever_readers = true; }
    else                        { p->writers++; p->ever_writers = true; }
    spin_unlock(&fifo_attach_lock);

    // POSIX FIFO open semantics:
    //   - O_RDONLY blocks until a writer is present (so the reader doesn't
    //     observe an empty pipe + writers==0 and immediately get EOF).
    //   - O_WRONLY blocks until a reader is present (so the writer doesn't
    //     blast SIGPIPE on a buffer with no readers).
    // Wake any peer parked on the opposite queue first — they were waiting
    // for *us*. Then, if the *other* side count is still 0, park ourselves.
    task_t* peer_chain = NULL;
    if (side == PIPE_SIDE_READ) {
        peer_chain = waiter_drain_all(&p->write_waiter, &p->write_waiter_tail);
    } else {
        peer_chain = waiter_drain_all(&p->read_waiter, &p->read_waiter_tail);
    }

    while (1) {
        // Use the sticky "ever had a peer" flag, not the live counter.
        // A peer that opens-and-closes between schedule() and our resume
        // would otherwise put us back to sleep forever.
        bool need_block = (side == PIPE_SIDE_READ) ? !p->ever_writers
                                                   : !p->ever_readers;
        if (!need_block) break;

        task_t* me = this_cpu()->current;
        if (side == PIPE_SIDE_READ) {
            waiter_enqueue(&p->read_waiter, &p->read_waiter_tail, me);
        } else {
            waiter_enqueue(&p->write_waiter, &p->write_waiter_tail, me);
        }
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&p->lock);

        // Wake any peer we collected before parking, so they can satisfy
        // *our* condition. (First iteration only: subsequent iterations
        // have nothing to wake.)
        while (peer_chain) {
            task_t* nxt = peer_chain->next;
            peer_chain->next = NULL;
            rq_enqueue_external(peer_chain);
            peer_chain = nxt;
        }

        schedule();

        CliHelper();
        spin_lock(&p->lock);
        if (side == PIPE_SIDE_READ) {
            waiter_remove_self(&p->read_waiter, &p->read_waiter_tail, me);
        } else {
            waiter_remove_self(&p->write_waiter, &p->write_waiter_tail, me);
        }
    }

    spin_unlock(&p->lock);

    while (peer_chain) {
        task_t* nxt = peer_chain->next;
        peer_chain->next = NULL;
        rq_enqueue_external(peer_chain);
        peer_chain = nxt;
    }
    if (ie) StiHelper();

    int r = install_end(f, p, side);
    if (r < 0) {
        // Roll back the readers/writers bump we just did. Take
        // fifo_attach_lock around the inode->pipe clear so a concurrent
        // fifo_open can't acquire p between our last-ref decrement and
        // pipe_free.
        ie = check_interrupts(); CliHelper();
        spin_lock(&fifo_attach_lock);
        spin_lock(&p->lock);
        if (side == PIPE_SIDE_READ) p->readers--;
        else                        p->writers--;
        bool free_pipe = (p->readers == 0 && p->writers == 0);
        if (free_pipe) in->pipe = NULL;
        spin_unlock(&p->lock);
        spin_unlock(&fifo_attach_lock);
        if (free_pipe) pipe_free(p);
        if (ie) StiHelper();
        return r;
    }
    // install_end overwrote f->inode with the FIFO inode pointer; that's
    // what we want — vfs_check_file is happy and pipe_close knows to clear
    // inode->pipe when the pipe is freed.
    return 0;
}
