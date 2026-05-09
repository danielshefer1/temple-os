#include "irq_handler.h"
#include "scheduler.h"
#include "paging.h"
#include "signal.h"
#include "tty.h"
#include "vt.h"
#include "kbd_dev.h"

static const char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

static const char special_chars[] = {
    ')', '!', '@', '#', '$', '%', '^', '&', '*', '('
};

void irq_handler(interrupt_frame_t* frame) {
    lapic[0xB0 / 4] = 0;

    switch (frame->int_no) {
        case 32: TimerHandler();         break;
        case 33: KeyboardHandler();      break;
        case 64: AhciHandler();          break;
        case 65: TlbShootdownHandler();  break;
        default:
            kprintf("Unkown Interrupt just fired, interrupt number: %d\n", frame->int_no);
            break;
    }

    // Deliver any pending signal before IRETing back to user. signal_deliver
    // is a no-op when frame->cs != 0x23 (i.e. we interrupted kernel mode),
    // so this is safe regardless of where the IRQ landed. With this hook a
    // CPU-bound user task with no syscalls still picks up signals on the
    // next timer tick (~1 ms).
    signal_deliver_on_return(frame);
}

void TimerHandler() {
    uint8_t cpu_id = get_cpuid();
    timer_ticks[cpu_id]++;
    scheduler_tick();
}

// Modifier + extended-prefix tracking. The kbd_us table maps every modifier
// scancode (Ctrl, Alt, Shift, Caps) to 0; we track the make/break state of
// each ourselves. PS/2 delivers extended (e.g. arrow / right-side) keys as
// a 0xE0 byte followed by a regular scancode in the next IRQ — we latch
// `extended` for one byte to disambiguate.
#define LEFT_ALT_SCANCODE  0x38
#define LEFT_CTRL_SCANCODE 0x1D
#define F1_SCANCODE        0x3B
#define F10_SCANCODE       0x44
#define F11_SCANCODE       0x57
#define F12_SCANCODE       0x58
#define KBD_EXTENDED       0xE0

static bool alt_pressed  = false;
static bool ctrl_pressed = false;
static bool extended     = false;

// xterm-style escape sequences for non-ASCII keys. tty_input_byte gets each
// byte one at a time; readers in raw mode reassemble. Cooked-mode echo will
// look ugly for these (the bracket and tail bytes show up literally) — that
// resolves itself once raw-mode programs (the future shell, line editors)
// drive the tty.
static void emit_seq(const char* s) {
    while (*s) tty_input_byte(&console_tty, *s++);
}

static void emit_extended(uint8_t code) {
    switch (code) {
        case 0x48: emit_seq("\x1b[A");  break;  // Up
        case 0x50: emit_seq("\x1b[B");  break;  // Down
        case 0x4D: emit_seq("\x1b[C");  break;  // Right
        case 0x4B: emit_seq("\x1b[D");  break;  // Left
        case 0x47: emit_seq("\x1b[H");  break;  // Home
        case 0x4F: emit_seq("\x1b[F");  break;  // End
        case 0x49: emit_seq("\x1b[5~"); break;  // PgUp
        case 0x51: emit_seq("\x1b[6~"); break;  // PgDn
        case 0x52: emit_seq("\x1b[2~"); break;  // Insert
        case 0x53: emit_seq("\x1b[3~"); break;  // Delete
        default: break;
    }
}

static const char* const fkey_seq[12] = {
    "\x1bOP",   "\x1bOQ",   "\x1bOR",   "\x1bOS",      // F1-F4
    "\x1b[15~", "\x1b[17~", "\x1b[18~", "\x1b[19~",    // F5-F8
    "\x1b[20~", "\x1b[21~", "\x1b[23~", "\x1b[24~",    // F9-F12
};

void KeyboardHandler() {
    uint8_t scancode = inb(0x60);

    // /dev/kbd hijack: when the userspace terminal has the device open, hand
    // it raw scancodes (including 0xE0 prefix bytes — the userspace term
    // does its own modifier tracking and key translation). The console TTY
    // path is dormant until /dev/kbd closes.
    if (kbd_dev_active()) {
        kbd_dev_input(scancode);
        return;
    }

    // Extended-scancode prefix: latch and wait for the second byte.
    if (scancode == KBD_EXTENDED) {
        extended = true;
        return;
    }

    uint8_t presscode    = scancode & 0x7F;
    bool    is_release   = scancode & 0x80;
    bool    was_extended = extended;
    extended = false;

    // ---- modifiers (no echo, no tty input) -------------------------------
    if (presscode == LEFT_ALT_SCANCODE) {        // covers Right Alt too: same presscode under E0
        alt_pressed = !is_release;
        return;
    }
    if (presscode == LEFT_CTRL_SCANCODE) {       // covers Right Ctrl under E0
        ctrl_pressed = !is_release;
        return;
    }
    if (presscode == LEFT_SHIFT_MAKE_SCANCODE || presscode == RIGHT_SHIFT_MAKE_SCANCODE) {
        console_tty.shift_pressed = !is_release;
        return;
    }

    if (is_release) return;

    // ---- Alt+F1..F6 → VT switch (must come before regular F-key emit) ----
    if (alt_pressed &&
        presscode >= F1_SCANCODE && presscode < F1_SCANCODE + NUM_VTS) {
        vt_switch_to((uint64_t)(presscode - F1_SCANCODE));
        return;
    }

    // ---- Extended (arrow / nav) keys -------------------------------------
    if (was_extended) {
        emit_extended(presscode);
        return;
    }

    // ---- Function keys (xterm sequences) ---------------------------------
    if (presscode >= F1_SCANCODE && presscode <= F10_SCANCODE) {
        emit_seq(fkey_seq[presscode - F1_SCANCODE]);
        return;
    }
    if (presscode == F11_SCANCODE) { emit_seq(fkey_seq[10]); return; }
    if (presscode == F12_SCANCODE) { emit_seq(fkey_seq[11]); return; }

    // ---- Plain ASCII (with shift / ctrl modifiers) -----------------------
    char c = kbd_us[presscode];
    if (c == 0) return;

    if (c == 'Q' || (console_tty.shift_pressed && c == 'q')) {
        Shutdown();
    }
    if (console_tty.shift_pressed) {
        if (c <= 'z' && c >= 'a') c -= 32;
        else {
            uint8_t special_idx = c - 48;
            c = special_chars[special_idx];
        }
    }
    if (ctrl_pressed) {
        // Ctrl+letter → 0x01..0x1A. Ctrl+@ / Ctrl+[ / Ctrl+\ / Ctrl+] /
        // Ctrl+^ / Ctrl+_ also fall in the C0 range, but those need their
        // raw-key scancodes which we don't reliably distinguish today; a
        // letter is enough to drive the typical Ctrl+C / Ctrl+D flow.
        if (c >= 'a' && c <= 'z') c = c - 'a' + 1;
        else if (c >= 'A' && c <= 'Z') c = c - 'A' + 1;
    }
    tty_input_byte(&console_tty, c);
}

// AhciHandler lives in drivers/ahci_driver.c so it can see static port_states.
