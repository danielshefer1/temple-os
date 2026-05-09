#include "kbd_dev.h"
#include "devfs.h"
#include "vfs_types.h"
#include "vfs_defs.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "vfs_file.h"
#include "string.h"
#include "extern.h"
#include "defintions.h"

#define KBD_RING_SIZE 256

typedef struct kbd_dev_t {
    spinlock_t lock;
    uint8_t    ring[KBD_RING_SIZE];
    uint64_t   head;
    uint64_t   tail;
    bool       open;
    task_t*    waiter;
    task_t*    waiter_tail;
} kbd_dev_t;

static kbd_dev_t kbd;
static inode_t   kbd_stub_inode;

bool kbd_dev_active(void) {
    return kbd.open;
}

void kbd_dev_input(uint8_t scancode) {
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&kbd.lock);
    uint64_t count = kbd.head - kbd.tail;
    if (count < KBD_RING_SIZE) {
        kbd.ring[kbd.head % KBD_RING_SIZE] = scancode;
        kbd.head++;
    }
    task_t* w = kbd.waiter;
    if (w) {
        kbd.waiter = w->next;
        if (kbd.waiter) kbd.waiter->prev = NULL;
        else            kbd.waiter_tail = NULL;
        w->next = w->prev = NULL;
        w->state = TASK_STATE_READY;
    }
    spin_unlock(&kbd.lock);
    if (w) rq_enqueue_external(w);
    if (ie) StiHelper();
}

static int64_t kbd_read(file_t* f, void* buf, uint64_t size) {
    (void)f;
    if (size == 0) return 0;
    uint8_t* dst = (uint8_t*)buf;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&kbd.lock);

    while (1) {
        uint64_t count = kbd.head - kbd.tail;
        if (count > 0) {
            uint64_t out = 0;
            while (kbd.tail != kbd.head && out < size) {
                dst[out++] = kbd.ring[kbd.tail % KBD_RING_SIZE];
                kbd.tail++;
            }
            spin_unlock(&kbd.lock);
            if (ie) StiHelper();
            return (int64_t)out;
        }
        task_t* me = this_cpu()->current;
        me->next = NULL;
        me->prev = kbd.waiter_tail;
        if (kbd.waiter_tail) kbd.waiter_tail->next = me;
        else                 kbd.waiter = me;
        kbd.waiter_tail = me;
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&kbd.lock);

        schedule();

        CliHelper();
        spin_lock(&kbd.lock);
        // Splice self out if signal-resumed.
        if (me->next || me->prev || kbd.waiter == me) {
            if (me->prev) me->prev->next = me->next;
            else if (kbd.waiter == me) kbd.waiter = me->next;
            if (me->next) me->next->prev = me->prev;
            else if (kbd.waiter_tail == me) kbd.waiter_tail = me->prev;
            me->next = me->prev = NULL;
        }
    }
}

static int64_t kbd_open(inode_t* in, file_t* f) {
    (void)in; (void)f;
    bool ie = check_interrupts();
    CliHelper();
    int64_t r = 0;
    spin_lock(&kbd.lock);
    if (kbd.open) r = -EBUSY;
    else          kbd.open = true;
    spin_unlock(&kbd.lock);
    if (ie) StiHelper();
    return r;
}

static int64_t kbd_close(file_t* f) {
    (void)f;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&kbd.lock);
    kbd.open = false;
    // Drain the ring on close so the next opener starts fresh.
    kbd.head = kbd.tail;
    spin_unlock(&kbd.lock);
    if (ie) StiHelper();
    return 0;
}

static file_ops_t kbd_fops = {
    .read  = kbd_read,
    .open  = kbd_open,
    .close = kbd_close,
};

void kbd_dev_init(void) {
    memset(&kbd, 0, sizeof(kbd));
    memset(&kbd_stub_inode, 0, sizeof(kbd_stub_inode));
    kbd_stub_inode.type = VFS_TYPE_CHARDEV;
    devfs_register_char(13, 0, &kbd_fops, NULL);
}
