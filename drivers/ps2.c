#include "ps2.h"
#include "extern.h"
#include "vga.h"

// 8042 controller registers
#define PS2_DATA        0x60
#define PS2_STATUS      0x64    // read
#define PS2_CMD         0x64    // write

// Status register bits
#define PS2_STAT_OUT_FULL  0x01  // data available at 0x60
#define PS2_STAT_IN_FULL   0x02  // 0x60/0x64 still holds host's last byte
#define PS2_STAT_AUX_DATA  0x20  // byte at 0x60 is from aux device (mouse)

// Controller commands
#define PS2_CMD_DISABLE_KBD     0xAD
#define PS2_CMD_ENABLE_KBD      0xAE
#define PS2_CMD_DISABLE_AUX     0xA7
#define PS2_CMD_ENABLE_AUX      0xA8
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_WRITE_AUX       0xD4   // next byte to 0x60 goes to mouse

// Config byte bits
#define PS2_CFG_KBD_IRQ         0x01
#define PS2_CFG_AUX_IRQ         0x02
#define PS2_CFG_AUX_CLOCK_OFF   0x20   // 1 = aux clock disabled

// Mouse commands
#define MOUSE_CMD_SET_DEFAULTS  0xF6
#define MOUSE_CMD_ENABLE_STREAM 0xF4
#define MOUSE_ACK               0xFA

// Pre-IRQ polling: short busy-loop bound. We're called from start(); no
// scheduler yet, so a hard spin is fine.
#define PS2_POLL_LIMIT 100000

static int64_t wait_input_empty(void) {
    for (uint64_t i = 0; i < PS2_POLL_LIMIT; i++) {
        if ((inb(PS2_STATUS) & PS2_STAT_IN_FULL) == 0) return 0;
    }
    return -1;
}

static int64_t wait_output_full(void) {
    for (uint64_t i = 0; i < PS2_POLL_LIMIT; i++) {
        if (inb(PS2_STATUS) & PS2_STAT_OUT_FULL) return 0;
    }
    return -1;
}

static int64_t cmd(uint8_t c) {
    if (wait_input_empty() < 0) return -1;
    outb(PS2_CMD, c);
    return 0;
}

static int64_t write_data(uint8_t b) {
    if (wait_input_empty() < 0) return -1;
    outb(PS2_DATA, b);
    return 0;
}

static int64_t read_data(uint8_t* out) {
    if (wait_output_full() < 0) return -1;
    *out = inb(PS2_DATA);
    return 0;
}

// Send a byte to the auxiliary device and read its ACK (0xFA).
static int64_t aux_send(uint8_t b) {
    if (cmd(PS2_CMD_WRITE_AUX) < 0) return -1;
    if (write_data(b) < 0) return -1;
    uint8_t resp = 0;
    if (read_data(&resp) < 0) return -1;
    return resp == MOUSE_ACK ? 0 : -1;
}

int64_t ps2_init_aux(void) {
    // Quiesce both ports so device chatter doesn't interleave with config.
    cmd(PS2_CMD_DISABLE_KBD);
    cmd(PS2_CMD_DISABLE_AUX);

    // Drain anything stale in the output buffer.
    while (inb(PS2_STATUS) & PS2_STAT_OUT_FULL) (void)inb(PS2_DATA);

    // Read config, enable aux IRQ + aux clock, write back.
    if (cmd(PS2_CMD_READ_CONFIG) < 0) return -1;
    uint8_t cfg = 0;
    if (read_data(&cfg) < 0) return -1;
    cfg |=  PS2_CFG_AUX_IRQ;
    cfg &= ~PS2_CFG_AUX_CLOCK_OFF;
    if (cmd(PS2_CMD_WRITE_CONFIG) < 0) return -1;
    if (write_data(cfg) < 0) return -1;

    // Bring both ports back online.
    cmd(PS2_CMD_ENABLE_KBD);
    cmd(PS2_CMD_ENABLE_AUX);

    // Mouse-side init. F6 returns the mouse to default settings (100Hz,
    // resolution 4 counts/mm, 1:1 scaling, streaming off). F4 turns
    // streaming on so motion/click bytes start flowing on IRQ12.
    if (aux_send(MOUSE_CMD_SET_DEFAULTS)  < 0) { kprintf("ps2: mouse F6 failed\n");  return -1; }
    if (aux_send(MOUSE_CMD_ENABLE_STREAM) < 0) { kprintf("ps2: mouse F4 failed\n");  return -1; }
    return 0;
}
