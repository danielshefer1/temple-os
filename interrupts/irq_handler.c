#include "irq_handler.h"
#include "scheduler.h"
#include "paging.h"
#include "signal.h"
#include "tty.h"

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

void KeyboardHandler() {
    uint8_t scancode = inb(0x60);
    uint8_t presscode = scancode & 0x7F;
    bool is_release = scancode & 0x80;
    char c = kbd_us[presscode];

    if  (presscode == LEFT_SHIFT_MAKE_SCANCODE || presscode == RIGHT_SHIFT_MAKE_SCANCODE) {
        console_tty.shift_pressed = !is_release;
        return;
    }
    if (c == 0) return;

    if (!is_release) {
        if (c == 'Q' || (console_tty.shift_pressed && c == 'q')) {
            // Preserve the legacy Shift+Q shutdown shortcut handled by the
            // old PushKeyboardBuffer path. With the tty in cooked mode the
            // byte would otherwise wait for a newline.
            Shutdown();
        }
        if (console_tty.shift_pressed) {
            if (c <= 'z' && c >= 'a') c -= 32;
            else {
                uint8_t special_idx = c - 48;
                c = special_chars[special_idx];
            }
        }
        tty_input_byte(&console_tty, c);
    }
}

// AhciHandler lives in drivers/ahci_driver.c so it can see static port_states.
