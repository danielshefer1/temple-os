#include "tty.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "vga.h"
#include "vfs_file.h"
#include "string.h"
#include "signal.h"
#include "extern.h"
#include "defintions.h"
#include "devfs.h"

// One global console tty. Becomes /dev/tty once devfs lands; until then it
// is reachable only via fd 0/1/2 which create_user_task wires up.
tty_t console_tty;

// Stub inode so vfs_check_file (which insists on a non-NULL inode whose
// type != VFS_TYPE_DIR) accepts tty file_t's.
static inode_t tty_stub_inode;

// ---- ring helpers (assume input_lock held) -------------------------------

static inline uint64_t ring_count(tty_t* t) { return t->head - t->tail; }

static inline void ring_push(tty_t* t, char c) {
    if (ring_count(t) >= TTY_BUF_SIZE) return;       // drop on overflow
    t->buf[t->head % TTY_BUF_SIZE] = c;
    t->head++;
}

static inline bool ring_pop_back(tty_t* t, char* out) {
    if (ring_count(t) == 0) return false;
    t->head--;
    if (out) *out = t->buf[t->head % TTY_BUF_SIZE];
    return true;
}

// Returns count copied. In ICANON, copies up through the first '\n' and
// only if a '\n' is present in the ring; otherwise returns 0. In raw mode,
// drains whatever is buffered up to `size`.
static int64_t drain_locked(tty_t* tty, char* dst, uint64_t size) {
    uint64_t out = 0;
    if (tty->flags & TTY_FLAG_ICANON) {
        bool has_nl = false;
        uint64_t nl_pos = 0;
        for (uint64_t i = tty->tail; i != tty->head; i++) {
            if (tty->buf[i % TTY_BUF_SIZE] == '\n') {
                has_nl = true;
                nl_pos = i;
                break;
            }
        }
        if (!has_nl) return 0;
        while (tty->tail <= nl_pos && out < size) {
            dst[out++] = tty->buf[tty->tail % TTY_BUF_SIZE];
            tty->tail++;
        }
        return (int64_t)out;
    }
    while (tty->tail != tty->head && out < size) {
        dst[out++] = tty->buf[tty->tail % TTY_BUF_SIZE];
        tty->tail++;
    }
    return (int64_t)out;
}

// ---- waiter queue (assume input_lock held) -------------------------------

static void waiter_enqueue_locked(tty_t* tty, task_t* me) {
    me->next = NULL;
    me->prev = tty->read_waiter_tail;
    if (tty->read_waiter_tail) tty->read_waiter_tail->next = me;
    else                       tty->read_waiter = me;
    tty->read_waiter_tail = me;
}

static task_t* waiter_pop_locked(tty_t* tty) {
    task_t* w = tty->read_waiter;
    if (!w) return NULL;
    tty->read_waiter = w->next;
    if (tty->read_waiter) tty->read_waiter->prev = NULL;
    else                  tty->read_waiter_tail = NULL;
    w->next = w->prev = NULL;
    return w;
}

// ---- file_ops ------------------------------------------------------------

static int64_t tty_read(file_t* f, void* buf, uint64_t size) {
    if (size == 0) return 0;
    tty_t* tty = (tty_t*)f->private_data;
    char* dst = (char*)buf;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&tty->input_lock);

    while (1) {
        int64_t n = drain_locked(tty, dst, size);
        if (n > 0) {
            spin_unlock(&tty->input_lock);
            if (ie) StiHelper();
            return n;
        }
        // No data ready — block until a wakeup or a signal arrives.
        task_t* me = this_cpu()->current;
        waiter_enqueue_locked(tty, me);
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&tty->input_lock);

        schedule();

        // Resumed. If a signal made us READY without anyone draining the
        // wait list (signal_send doesn't know about us), splice ourselves
        // out before retrying so we don't sit in the queue forever.
        CliHelper();
        spin_lock(&tty->input_lock);
        if (me->next || me->prev || tty->read_waiter == me) {
            if (me->prev) me->prev->next = me->next;
            else if (tty->read_waiter == me) tty->read_waiter = me->next;
            if (me->next) me->next->prev = me->prev;
            else if (tty->read_waiter_tail == me) tty->read_waiter_tail = me->prev;
            me->next = me->prev = NULL;
        }
    }
}

static int64_t tty_write(file_t* f, const void* buf, uint64_t size) {
    (void)f;
    return (int64_t)print_str_SYSCALL((const char*)buf, GREY_COLOR, size);
}

static int64_t tty_ioctl(file_t* f, uint64_t cmd, void* arg) {
    (void)arg;
    tty_t* tty = (tty_t*)f->private_data;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&tty->input_lock);
    int64_t r = 0;
    switch (cmd) {
        case TTY_IOCTL_SET_RAW:
            tty->flags &= ~(TTY_FLAG_ICANON | TTY_FLAG_ECHO);
            break;
        case TTY_IOCTL_SET_COOKED:
            tty->flags |= TTY_FLAG_ICANON | TTY_FLAG_ECHO;
            break;
        case TTY_IOCTL_SET_FOREGROUND:
            // Legacy alias: treat the calling task's pgid as the new fg pgrp.
            tty->pgrp = this_cpu()->current->pgid;
            break;
        case TTY_IOCTL_TIOCSPGRP:
            tty->pgrp = (uint64_t)arg;
            break;
        case TTY_IOCTL_TIOCGPGRP:
            if (arg) *(uint64_t*)arg = tty->pgrp;
            break;
        default:
            r = -ENOTTY;
            break;
    }
    spin_unlock(&tty->input_lock);
    if (ie) StiHelper();
    return r;
}

static int64_t tty_close(file_t* f) {
    (void)f;
    return 0;
}

static file_ops_t tty_fops = {
    .read     = tty_read,
    .write    = tty_write,
    .seek     = NULL,
    .truncate = NULL,
    .readdir  = NULL,
    .open     = NULL,
    .close    = tty_close,
    .flush    = NULL,
    .ioctl    = tty_ioctl,
};

// ---- public ---------------------------------------------------------------

void tty_init(tty_t* tty) {
    memset(tty, 0, sizeof(*tty));
    tty->flags = TTY_FLAG_ICANON | TTY_FLAG_ECHO | TTY_FLAG_ISIG;
    tty->fops = &tty_fops;

    // Stub inode shared by every tty file_t opened via tty_open() (i.e.
    // create_user_task fd 0/1/2 wiring). vfs_read/write/ioctl only consult
    // f->ops (= tty_fops) and reject the directory case.
    tty_stub_inode.type = VFS_TYPE_CHARDEV;

    // Register with devfs so user code can also open the tty by path
    // (e.g. open("/dev/tty", O_RDWR)). When opening that route, vfs_open
    // looks up (4, 0) and uses these same fops, with token == &console_tty
    // ending up in file_t.private_data — matching tty_open()'s convention.
    if (tty == &console_tty) {
        devfs_register_char(4, 0, &tty_fops, tty);
    }
}

file_t* tty_open(tty_t* tty) {
    file_t* f = vfs_file_alloc();
    if (!f) return NULL;
    f->inode        = &tty_stub_inode;
    f->ops          = tty->fops;
    f->position     = 0;
    f->flags        = 0;
    f->mode         = 0;
    f->ref_count    = 1;
    f->private_data = tty;
    return f;
}

void tty_drop_task(task_t* t) {
    // With process groups, the tty no longer holds a per-task pointer.
    // Clearing the foreground pgrp when the *leader* of that group exits
    // would be reasonable, but we don't track membership cardinality and
    // setpgid+children-still-alive is the more common case. Leave the pgrp
    // value in place; signal_send_pgrp gracefully handles the empty case.
    (void)t;
}

// IRQ-context producer. Called from KeyboardHandler.
void tty_input_byte(tty_t* tty, char c) {
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&tty->input_lock);

    // Ctrl+C: never enters the buffer; signals every task in the foreground
    // process group. signal_send_pgrp walks scheduler state, so it must run
    // outside our own input_lock to avoid lock-ordering problems with the
    // run-queue locks it acquires.
    if ((tty->flags & TTY_FLAG_ISIG) && c == 0x03) {
        uint64_t pgrp = tty->pgrp;
        spin_unlock(&tty->input_lock);
        if (pgrp != 0) signal_send_pgrp(pgrp, SIGINT);
        if (ie) StiHelper();
        return;
    }

    // Cooked-mode backspace: erase last unread char (and echo a destructive
    // BS). Don't underflow into already-consumed bytes.
    if ((tty->flags & TTY_FLAG_ICANON) && c == '\b') {
        char popped;
        if (ring_pop_back(tty, &popped) && (tty->flags & TTY_FLAG_ECHO)) {
            putchar('\b', GREY_COLOR);
        }
        spin_unlock(&tty->input_lock);
        if (ie) StiHelper();
        return;
    }

    ring_push(tty, c);

    if (tty->flags & TTY_FLAG_ECHO) {
        putchar(c, GREY_COLOR);
    }

    // In ICANON the reader can only progress on '\n'; in raw mode every
    // byte may unblock. Wake at most one waiter — readers re-check on
    // resume so spurious wakes are harmless.
    bool wake = (tty->flags & TTY_FLAG_ICANON) ? (c == '\n') : true;
    task_t* w = wake ? waiter_pop_locked(tty) : NULL;
    if (w) w->state = TASK_STATE_READY;

    spin_unlock(&tty->input_lock);
    if (w) rq_enqueue_external(w);
    if (ie) StiHelper();
}
