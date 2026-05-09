#include "pty.h"
#include "tty_ldisc.h"
#include "tty_types.h"          // TTY_FLAG_*, winsize_t
#include "devfs.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "vfs_file.h"
#include "string.h"
#include "signal.h"
#include "extern.h"
#include "defintions.h"

// Single global table: indexed by /dev/pts minor. ptmx_open() finds a free
// slot; slaves are reached by name via /dev/pts/N which devfs resolves to
// (PTY_SLAVE_MAJOR, N) → pty_slave_ops + token = &pty_table[N].
static pty_pair_t pty_table[PTY_MAX_PAIRS];

// Stub inode for ptmx-allocated master file_t's. Same trick as
// tty_stub_inode in drivers/tty.c.
static inode_t pty_master_stub_inode;

pty_pair_t* pty_get(uint16_t index) {
    if (index >= PTY_MAX_PAIRS) return NULL;
    pty_pair_t* p = &pty_table[index];
    return p->in_use ? p : NULL;
}

// ---- ring views (caller holds p->lock) ----------------------------------

static ldisc_ring_t m2s_ring(pty_pair_t* p) {
    return (ldisc_ring_t){
        .buf         = p->m2s,
        .size        = PTY_BUF_SIZE,
        .head        = &p->m2s_head,
        .tail        = &p->m2s_tail,
        .waiter      = &p->s_read_waiter,
        .waiter_tail = &p->s_read_waiter_tail,
    };
}

static ldisc_ring_t s2m_ring(pty_pair_t* p) {
    return (ldisc_ring_t){
        .buf         = p->s2m,
        .size        = PTY_BUF_SIZE,
        .head        = &p->s2m_head,
        .tail        = &p->s2m_tail,
        .waiter      = &p->m_read_waiter,
        .waiter_tail = &p->m_read_waiter_tail,
    };
}

// Echo callback for the line discipline on master input: append to s2m so
// the userspace term sees the typed-byte echo and renders it.
static void echo_to_master(void* ctx, char c) {
    ldisc_ring_t* sring = (ldisc_ring_t*)ctx;
    ldisc_push(sring, c);
    // Wake whoever's blocked reading the master.
    task_t* w = ldisc_waiter_pop(sring);
    if (w) {
        w->state = TASK_STATE_READY;
        // We can't drop p->lock here (caller still holds it), but enqueueing
        // to the run queue is safe: rq_enqueue_external takes its own lock.
        rq_enqueue_external(w);
    }
}

// ---- slave file_ops -----------------------------------------------------

// Slave read: drain m2s through the line discipline (so cooked-mode reads
// only complete on '\n'). Blocks if no data and master is open.
static int64_t pty_slave_read(file_t* f, void* buf, uint64_t size) {
    if (size == 0) return 0;
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    char* dst = (char*)buf;
    ldisc_ring_t r = m2s_ring(p);

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);

    while (1) {
        int64_t n = ldisc_drain(&r, p->flags, dst, size);
        if (n > 0) {
            spin_unlock(&p->lock);
            if (ie) StiHelper();
            return n;
        }
        if (!p->master_open) {
            // EOF — userspace term closed the master.
            spin_unlock(&p->lock);
            if (ie) StiHelper();
            return 0;
        }
        task_t* me = this_cpu()->current;
        ldisc_waiter_enqueue(&r, me);
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&p->lock);

        schedule();

        CliHelper();
        spin_lock(&p->lock);
        ldisc_waiter_remove(&r, me);
    }
}

// Slave write: bytes go raw into s2m so the master (userspace term) reads
// them and renders. Wakes one master reader per call.
static int64_t pty_slave_write(file_t* f, const void* buf, uint64_t size) {
    if (size == 0) return 0;
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    const char* src = (const char*)buf;
    ldisc_ring_t r = s2m_ring(p);
    uint64_t total = 0;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);

    while (total < size) {
        if (!p->master_open) {
            spin_unlock(&p->lock);
            // Default action of SIGPIPE is termination — same convention as
            // a closed pipe write end.
            signal_send(this_cpu()->current, SIGPIPE);
            if (ie) StiHelper();
            return (total > 0) ? (int64_t)total : -EPIPE;
        }
        uint64_t space = PTY_BUF_SIZE - (p->s2m_head - p->s2m_tail);
        if (space == 0) {
            // Drop overflow on the floor instead of blocking — keeps the
            // slave from deadlocking when the userspace term is slow. Real
            // ptys block; this is acceptable for v1.
            break;
        }
        uint64_t want = size - total;
        if (want > space) want = space;
        for (uint64_t i = 0; i < want; i++) ldisc_push(&r, src[total + i]);
        total += want;

        task_t* w = ldisc_waiter_pop(&r);
        if (w) {
            w->state = TASK_STATE_READY;
            rq_enqueue_external(w);
        }
    }

    spin_unlock(&p->lock);
    if (ie) StiHelper();
    return (int64_t)total;
}

static int64_t pty_slave_ioctl(file_t* f, uint64_t cmd, void* arg) {
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);
    int64_t r = 0;
    switch (cmd) {
        case TTY_IOCTL_TIOCSPGRP:
            p->pgrp = (uint64_t)arg;
            break;
        case TTY_IOCTL_TIOCGPGRP:
            if (arg) *(uint64_t*)arg = p->pgrp;
            break;
        case TTY_IOCTL_TIOCGWINSZ: {
            if (!arg) { r = -EINVAL; break; }
            winsize_t* ws = (winsize_t*)arg;
            ws->ws_row    = p->rows;
            ws->ws_col    = p->cols;
            ws->ws_xpixel = p->xpixel;
            ws->ws_ypixel = p->ypixel;
            break;
        }
        case TIOCSWINSZ: {
            if (!arg) { r = -EINVAL; break; }
            winsize_t* ws = (winsize_t*)arg;
            p->rows   = ws->ws_row;
            p->cols   = ws->ws_col;
            p->xpixel = ws->ws_xpixel;
            p->ypixel = ws->ws_ypixel;
            uint64_t pgrp = p->pgrp;
            spin_unlock(&p->lock);
            if (pgrp != 0) signal_send_pgrp(pgrp, SIGWINCH);
            if (ie) StiHelper();
            return 0;
        }
        case TIOCSCTTY: {
            // Make this fd the calling task's controlling terminal. We don't
            // enforce session-leader restrictions in this OS; any task may
            // claim a ctty. file_t ref is borrowed (we don't bump refcount;
            // caller's fd keeps it alive).
            task_t* me = this_cpu()->current;
            me->ctty = f;
            // Adopt this task's pgrp as the foreground if none set.
            if (p->pgrp == 0) p->pgrp = me->pgid;
            break;
        }
        case TTY_IOCTL_SET_RAW:
            p->flags &= ~(TTY_FLAG_ICANON | TTY_FLAG_ECHO);
            break;
        case TTY_IOCTL_SET_COOKED:
            p->flags |= TTY_FLAG_ICANON | TTY_FLAG_ECHO;
            break;
        default:
            r = -ENOTTY;
            break;
    }
    spin_unlock(&p->lock);
    if (ie) StiHelper();
    return r;
}

static int64_t pty_slave_open(inode_t* in, file_t* f) {
    (void)in;
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    if (!p || !p->in_use) return -ENODEV;
    if (p->locked) return -EIO;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);
    p->slave_open = true;
    p->slave_ever_opened = true;
    // Wake any master reader that was blocked waiting for the slave to
    // attach. Without this, the userspace term parent could enter pty
    // master_read before the shell child finishes opening /dev/pts/N and
    // race-loop on the !slave_open EOF path. (We changed master_read to
    // gate EOF on slave_ever_opened, so the master will block from now
    // on; we still wake an existing waiter so it re-enters the drain.)
    ldisc_ring_t s = s2m_ring(p);
    task_t* w = ldisc_waiter_pop(&s);
    spin_unlock(&p->lock);
    if (w) {
        w->state = TASK_STATE_READY;
        rq_enqueue_external(w);
    }
    if (ie) StiHelper();
    return 0;
}

static int64_t pty_slave_close(file_t* f) {
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    if (!p) return 0;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);
    p->slave_open = false;
    // Wake any master reader so it sees EOF on s2m.
    ldisc_ring_t s = s2m_ring(p);
    task_t* w = ldisc_waiter_pop(&s);
    spin_unlock(&p->lock);
    if (w) {
        w->state = TASK_STATE_READY;
        rq_enqueue_external(w);
    }
    if (ie) StiHelper();
    return 0;
}

static file_ops_t pty_slave_ops = {
    .read  = pty_slave_read,
    .write = pty_slave_write,
    .ioctl = pty_slave_ioctl,
    .open  = pty_slave_open,
    .close = pty_slave_close,
};

// ---- master file_ops ----------------------------------------------------

// Master read: drain s2m raw. Blocks until slave writes or closes.
static int64_t pty_master_read(file_t* f, void* buf, uint64_t size) {
    if (size == 0) return 0;
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    char* dst = (char*)buf;
    ldisc_ring_t r = s2m_ring(p);

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);

    while (1) {
        // Raw drain regardless of flags.
        int64_t n = ldisc_drain(&r, 0, dst, size);
        if (n > 0) {
            spin_unlock(&p->lock);
            if (ie) StiHelper();
            return n;
        }
        // EOF only after the slave was opened at least once and is now
        // gone. Without the `slave_ever_opened` gate, a master_read that
        // races ahead of the slave's first open returns 0 immediately and
        // the userspace term's render loop exits before the shell ever
        // attaches.
        if (p->slave_ever_opened && !p->slave_open) {
            spin_unlock(&p->lock);
            if (ie) StiHelper();
            return 0;
        }
        task_t* me = this_cpu()->current;
        ldisc_waiter_enqueue(&r, me);
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&p->lock);

        schedule();

        CliHelper();
        spin_lock(&p->lock);
        ldisc_waiter_remove(&r, me);

        // If a signal woke us, return EINTR so userspace can handle it
        // (e.g. /bin/term's SIGALRM-driven cursor blink) instead of
        // re-blocking on the same waiter and swallowing the signal.
        if (__atomic_load_n(&me->pending_signals, __ATOMIC_RELAXED)) {
            spin_unlock(&p->lock);
            if (ie) StiHelper();
            return -EINTR;
        }
    }
}

// Master write: bytes are user keystrokes. Run them through the same line
// discipline the kernel TTY uses, with ECHO routed back to s2m so the
// userspace term shows what the user typed.
static int64_t pty_master_write(file_t* f, const void* buf, uint64_t size) {
    if (size == 0) return 0;
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    const char* src = (const char*)buf;
    ldisc_ring_t mring = m2s_ring(p);
    ldisc_ring_t sring = s2m_ring(p);
    uint64_t total = 0;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);

    for (; total < size; total++) {
        ldisc_result_t res;
        ldisc_input(p->flags, &mring, p->pgrp, src[total],
                    echo_to_master, &sring, &res);
        // Out-of-lock side effects collected as we go. Defer signal until we
        // can drop the lock. Wake (echo_to_master already did one); the
        // ldisc_input wake (slave reader unblock) we still need.
        if (res.wake) {
            res.wake->state = TASK_STATE_READY;
            rq_enqueue_external(res.wake);
        }
        if (res.signal && res.signal_pgrp != 0) {
            uint64_t pgrp = res.signal_pgrp;
            int sig = res.signal;
            spin_unlock(&p->lock);
            signal_send_pgrp(pgrp, sig);
            CliHelper();
            spin_lock(&p->lock);
        }
    }

    spin_unlock(&p->lock);
    if (ie) StiHelper();
    return (int64_t)total;
}

static int64_t pty_master_ioctl(file_t* f, uint64_t cmd, void* arg) {
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);
    int64_t r = 0;
    switch (cmd) {
        case TIOCGPTN:
            if (!arg) { r = -EINVAL; break; }
            *(uint32_t*)arg = (uint32_t)p->index;
            break;
        case TIOCSPTLCK:
            if (!arg) { r = -EINVAL; break; }
            p->locked = (*(int*)arg) != 0;
            break;
        case TIOCSWINSZ: {
            if (!arg) { r = -EINVAL; break; }
            winsize_t* ws = (winsize_t*)arg;
            p->rows   = ws->ws_row;
            p->cols   = ws->ws_col;
            p->xpixel = ws->ws_xpixel;
            p->ypixel = ws->ws_ypixel;
            uint64_t pgrp = p->pgrp;
            spin_unlock(&p->lock);
            if (pgrp != 0) signal_send_pgrp(pgrp, SIGWINCH);
            if (ie) StiHelper();
            return 0;
        }
        case TTY_IOCTL_TIOCGWINSZ: {
            if (!arg) { r = -EINVAL; break; }
            winsize_t* ws = (winsize_t*)arg;
            ws->ws_row    = p->rows;
            ws->ws_col    = p->cols;
            ws->ws_xpixel = p->xpixel;
            ws->ws_ypixel = p->ypixel;
            break;
        }
        default:
            r = -ENOTTY;
            break;
    }
    spin_unlock(&p->lock);
    if (ie) StiHelper();
    return r;
}

static int64_t pty_master_close(file_t* f) {
    pty_pair_t* p = (pty_pair_t*)f->private_data;
    if (!p) return 0;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&p->lock);
    p->master_open = false;
    // Wake any slave reader so it sees EOF on m2s.
    ldisc_ring_t m = m2s_ring(p);
    task_t* w = ldisc_waiter_pop(&m);
    bool free_slot = !p->slave_open && !p->master_open;
    if (free_slot) {
        memset(p, 0, sizeof(*p));
    }
    spin_unlock(&p->lock);
    if (w) {
        w->state = TASK_STATE_READY;
        rq_enqueue_external(w);
    }
    if (ie) StiHelper();
    return 0;
}

static file_ops_t pty_master_ops = {
    .read  = pty_master_read,
    .write = pty_master_write,
    .ioctl = pty_master_ioctl,
    .open  = NULL,           // ptmx_ops->open promotes to master_ops below
    .close = pty_master_close,
};

// ---- ptmx (factory) -----------------------------------------------------

// open("/dev/ptmx") lands here via the devfs CHARDEV path. Allocate a free
// pair, rebind f to the master ops, and stash the pair in private_data.
static int64_t ptmx_open(inode_t* in, file_t* f) {
    (void)in;
    bool ie = check_interrupts();
    CliHelper();
    pty_pair_t* p = NULL;
    for (uint16_t i = 0; i < PTY_MAX_PAIRS; i++) {
        if (!pty_table[i].in_use) {
            p = &pty_table[i];
            memset(p, 0, sizeof(*p));
            p->in_use      = true;
            p->master_open = true;
            p->locked      = true;
            p->index       = i;
            p->flags       = TTY_FLAG_ICANON | TTY_FLAG_ECHO | TTY_FLAG_ISIG;
            break;
        }
    }
    if (ie) StiHelper();
    if (!p) return -ENOMEM;

    f->ops          = &pty_master_ops;
    f->private_data = p;
    f->inode        = &pty_master_stub_inode;
    f->position     = 0;
    return 0;
}

static file_ops_t ptmx_ops = {
    .open  = ptmx_open,
    // read/write/ioctl/close never invoked through this struct because
    // ptmx_open swaps f->ops to pty_master_ops before vfs_open returns.
};

void pty_init(void) {
    memset(pty_table, 0, sizeof(pty_table));
    memset(&pty_master_stub_inode, 0, sizeof(pty_master_stub_inode));
    pty_master_stub_inode.type = VFS_TYPE_CHARDEV;

    devfs_register_char(PTY_PTMX_MAJOR, PTY_PTMX_MINOR, &ptmx_ops, NULL);
    for (uint16_t i = 0; i < PTY_MAX_PAIRS; i++) {
        devfs_register_char(PTY_SLAVE_MAJOR, i, &pty_slave_ops, &pty_table[i]);
    }
}
