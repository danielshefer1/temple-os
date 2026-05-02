#include "irq_handler.h"
#include "scheduler.h"

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
        case 32:
            TimerHandler();
            return;
        case 33:
            KeyboardHandler();
            return;
        case 64:
            AhciHandler();
            return;
        default:
            kprintf("Unkown Interrupt just fired, interrupt number: %d\n", frame->int_no);
    }
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
        shift_pressed = !is_release;
        return;
    }
    if (c == 0) return;

    if (!is_release) {
        if (shift_pressed) {
            if (c <= 'z' && c >= 'a') c -= 32;
            else {
                uint8_t special_idx = c - 48;
                c = special_chars[special_idx];
            }
        }
        PushKeyboardBuffer(&console_buffer, c);
    }
}

void AhciHandler() {
    uint32_t interrupt_status = hba->is;


    for (int i = 0; i < 32; i++) {
        if (interrupt_status & (1 << i)) {
            hba_port_t* port = &hba->ports[i];


            uint32_t port_is = port->is;


            if (port_is & (1 << 5)) {

            }
            if (port_is & (1 << 0)) {

            }
            if (port_is & (1 << 30)) {
                kprintf("AHCI Error on port %d, Error Code: %x\n", i, port->is);
                uint32_t tfd = port->tfd;
                uint8_t error_reg = (tfd >> 8) & 0xFF;
                kprintf("TFES detected. Error Register: %x\n", error_reg);
            }

            // 5. CLEAR the port interrupts (Write 1s to the bits that were set)
            port->is = port_is;

            // 6. CLEAR the global status bit for this port
            hba->is = (1 << i);
        }
    }
}
