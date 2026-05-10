#include "mouse_dev.h"
#include "devfs.h"
#include "vfs_types.h"
#include "vfs_defs.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "vfs_file.h"
#include "string.h"
#include "extern.h"
#include "defintions.h"

// Ring of decoded events. 256 * 8 = 2 KB — generous; PS/2 streams at
// ~100 Hz so even a slow reader can disappear for a couple of seconds
// before we start dropping.
#define MOUSE_RING_SIZE 256

typedef struct mouse_dev_t {
    spinlock_t     lock;
    mouse_event_t  ring[MOUSE_RING_SIZE];
    uint64_t       head;
    uint64_t       tail;
    bool           open;
    task_t*        waiter;
    task_t*        waiter_tail;

    // Packet assembler state. PS/2 sends 3-byte packets; we collect them
    // here and decode on the third byte. `phase` advances 0→1→2→0.
    uint8_t        pkt[3];
    uint8_t        phase;
} mouse_dev_t;

static mouse_dev_t mouse;
static inode_t     mouse_stub_inode;

// Push one decoded event and wake one reader. Caller holds mouse.lock.
static void enqueue_event(mouse_event_t ev) {
    uint64_t count = mouse.head - mouse.tail;
    if (count < MOUSE_RING_SIZE) {
        mouse.ring[mouse.head % MOUSE_RING_SIZE] = ev;
        mouse.head++;
    }
}

// Decode pkt[0..2] into a mouse_event_t. Drops packets with the X/Y
// overflow bits set since the dx/dy values are meaningless then.
static bool decode_packet(mouse_event_t* out) {
    uint8_t b0 = mouse.pkt[0];
    if (b0 & 0xC0) return false;        // X or Y overflow
    int16_t dx = (int16_t)mouse.pkt[1];
    int16_t dy = (int16_t)mouse.pkt[2];
    if (b0 & 0x10) dx |= (int16_t)0xFF00;   // sign-extend 9-bit dx
    if (b0 & 0x20) dy |= (int16_t)0xFF00;   // sign-extend 9-bit dy
    out->dx      = dx;
    out->dy      = (int16_t)(-dy);          // PS/2 +y is up; flip to +y down
    out->buttons = b0 & 0x07;
    out->_pad[0] = out->_pad[1] = out->_pad[2] = 0;
    return true;
}

// IRQ-side producer: feed one byte from port 0x60 into the assembler.
static void feed_byte(uint8_t b) {
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&mouse.lock);

    // Sync: bit 3 of byte 0 is always 1 in a valid PS/2 packet. If we're
    // expecting byte 0 and that bit is clear, we're out of phase — drop
    // and wait for a real start byte.
    if (mouse.phase == 0 && (b & 0x08) == 0) {
        spin_unlock(&mouse.lock);
        if (ie) StiHelper();
        return;
    }

    mouse.pkt[mouse.phase++] = b;
    task_t* w = NULL;
    if (mouse.phase == 3) {
        mouse.phase = 0;
        mouse_event_t ev;
        if (decode_packet(&ev)) {
            enqueue_event(ev);
            w = mouse.waiter;
            if (w) {
                mouse.waiter = w->next;
                if (mouse.waiter) mouse.waiter->prev = NULL;
                else              mouse.waiter_tail = NULL;
                w->next = w->prev = NULL;
                w->state = TASK_STATE_READY;
            }
        }
    }

    spin_unlock(&mouse.lock);
    if (w) rq_enqueue_external(w);
    if (ie) StiHelper();
}

void MouseHandler(void) {
    // The 8042 multiplexes keyboard and aux on port 0x60. Bit 5 of the
    // status register tags bytes belonging to the aux device. When IRQ12
    // fires we drain only those bytes; if the status says no data is
    // available or it's a keyboard byte, leave it alone for IRQ1.
    for (uint64_t guard = 0; guard < 16; guard++) {
        uint8_t status = inb(0x64);
        if (!(status & 0x01)) return;
        if (!(status & 0x20)) return;
        uint8_t b = inb(0x60);
        feed_byte(b);
    }
}

static int64_t mouse_read(file_t* f, void* buf, uint64_t size) {
    (void)f;
    if (size < sizeof(mouse_event_t)) return 0;
    // Truncate to whole-event multiples — partial events would desync
    // userspace.
    size -= size % sizeof(mouse_event_t);
    uint8_t* dst = (uint8_t*)buf;

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&mouse.lock);

    while (1) {
        uint64_t count = mouse.head - mouse.tail;
        if (count > 0) {
            uint64_t out = 0;
            while (mouse.tail != mouse.head && out + sizeof(mouse_event_t) <= size) {
                memcpy(dst + out,
                       &mouse.ring[mouse.tail % MOUSE_RING_SIZE],
                       sizeof(mouse_event_t));
                out += sizeof(mouse_event_t);
                mouse.tail++;
            }
            spin_unlock(&mouse.lock);
            if (ie) StiHelper();
            return (int64_t)out;
        }
        task_t* me = this_cpu()->current;
        me->next = NULL;
        me->prev = mouse.waiter_tail;
        if (mouse.waiter_tail) mouse.waiter_tail->next = me;
        else                   mouse.waiter = me;
        mouse.waiter_tail = me;
        me->state = TASK_STATE_BLOCKED;
        spin_unlock(&mouse.lock);

        schedule();

        CliHelper();
        spin_lock(&mouse.lock);
        // Splice self out if signal-resumed.
        if (me->next || me->prev || mouse.waiter == me) {
            if (me->prev) me->prev->next = me->next;
            else if (mouse.waiter == me) mouse.waiter = me->next;
            if (me->next) me->next->prev = me->prev;
            else if (mouse.waiter_tail == me) mouse.waiter_tail = me->prev;
            me->next = me->prev = NULL;
        }
    }
}

static int64_t mouse_open(inode_t* in, file_t* f) {
    (void)in; (void)f;
    bool ie = check_interrupts();
    CliHelper();
    int64_t r = 0;
    spin_lock(&mouse.lock);
    if (mouse.open) r = -EBUSY;
    else            mouse.open = true;
    spin_unlock(&mouse.lock);
    if (ie) StiHelper();
    return r;
}

static int64_t mouse_close(file_t* f) {
    (void)f;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&mouse.lock);
    mouse.open = false;
    // Drain on close so the next opener starts from a clean stream and
    // reset the assembler in case we were mid-packet.
    mouse.head  = mouse.tail;
    mouse.phase = 0;
    spin_unlock(&mouse.lock);
    if (ie) StiHelper();
    return 0;
}

static file_ops_t mouse_fops = {
    .read  = mouse_read,
    .open  = mouse_open,
    .close = mouse_close,
};

void mouse_dev_init(void) {
    memset(&mouse, 0, sizeof(mouse));
    memset(&mouse_stub_inode, 0, sizeof(mouse_stub_inode));
    mouse_stub_inode.type = VFS_TYPE_CHARDEV;
    devfs_register_char(13, 1, &mouse_fops, NULL);
}
