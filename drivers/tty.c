#include "tty.h"
#include "tty_ldisc.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "vga.h"
#include "vfs_file.h"
#include "string.h"
#include "signal.h"
#include "extern.h"
#include "defintions.h"
#include "devfs.h"
#include "fb_console.h"
#include "global.h"

// One global console tty. Becomes /dev/tty once devfs lands; until then it
// is reachable only via fd 0/1/2 which create_user_task wires up.
tty_t console_tty;

// Stub inode so vfs_check_file (which insists on a non-NULL inode whose
// type != VFS_TYPE_DIR) accepts tty file_t's.
static inode_t tty_stub_inode;

// Build an ldisc_ring_t view of `tty`. tty_t carries inline storage; the
// view just packages pointers into it for the shared helpers in tty_ldisc.
static inline ldisc_ring_t tty_ring(tty_t* t) {
    return (ldisc_ring_t){
        .buf         = t->buf,
        .size        = TTY_BUF_SIZE,
        .head        = &t->head,
        .tail        = &t->tail,
        .waiter      = &t->read_waiter,
        .waiter_tail = &t->read_waiter_tail,
    };
}

// Echo callback for the line discipline: write to the framebuffer console.
static void tty_echo(void* ctx, char c) {
    (void)ctx;
    putchar(c, GREY_COLOR);
}

// ---- file_ops ------------------------------------------------------------

static int64_t tty_read(file_t* f, void* buf, uint64_t size) {
    if (size == 0) return 0;
    tty_t* tty = (tty_t*)f->private_data;
    char* dst = (char*)buf;
    ldisc_ring_t r = tty_ring(tty);

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&tty->input_lock);

    while (1) {
        int64_t n = ldisc_drain(&r, tty->flags, dst, size);
        if (n > 0) {
            spin_unlock(&tty->input_lock);
            if (ie) StiHelper();
            return n;
        }
        // No data ready — block until a wakeup or a signal arrives.
        task_t* me = this_cpu()->current;
        ldisc_waiter_enqueue(&r, me);
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&tty->input_lock);

        schedule();

        // Resumed. If a signal made us READY without anyone draining the
        // wait list (signal_send doesn't know about us), splice ourselves
        // out before retrying so we don't sit in the queue forever.
        CliHelper();
        spin_lock(&tty->input_lock);
        ldisc_waiter_remove(&r, me);
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
        case TTY_IOCTL_TIOCGWINSZ: {
            if (!arg) { r = -EINVAL; break; }
            uint64_t cols = 0, rows = 0;
            fb_console_geometry(&cols, &rows);
            winsize_t* ws = (winsize_t*)arg;
            ws->ws_row    = (uint16_t)rows;
            ws->ws_col    = (uint16_t)cols;
            ws->ws_xpixel = (uint16_t)fb_info.width;
            ws->ws_ypixel = (uint16_t)fb_info.height;
            break;
        }
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
    ldisc_ring_t r = tty_ring(tty);
    ldisc_result_t res;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&tty->input_lock);
    ldisc_input(tty->flags, &r, tty->pgrp, c, tty_echo, NULL, &res);
    spin_unlock(&tty->input_lock);

    // Out-of-lock side effects: signal_send_pgrp and rq_enqueue_external
    // both take run-queue locks; running them under input_lock would invert
    // lock order.
    if (res.signal && res.signal_pgrp != 0) {
        signal_send_pgrp(res.signal_pgrp, res.signal);
    }
    if (res.wake) {
        res.wake->state = TASK_STATE_READY;
        rq_enqueue_external(res.wake);
    }
    if (ie) StiHelper();
}
