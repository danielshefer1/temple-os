#include "vga.h"
#include "vt.h"

#define COM1_IER (COM1_BASE + 1)
#define COM1_LCR (COM1_BASE + 3)
#define COM1_MCR (COM1_BASE + 4)

static spinlock_t vga_spinlock = {0};

// IRQ-safe lock/unlock for the unified output path. Plain spin_lock leaves
// IF on, so an IRQ that runs on the same core mid-kprintf and itself does
// kernel output races at character granularity (visible as interleaved
// glyphs) or — worse — same-core deadlocks if the IRQ tries to re-acquire.
// Save/restore IF around the critical section so kprintf is atomic vs. all
// IRQ-context writers (timer logs, panic, keyboard echo).
static inline bool vga_lock(void) {
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&vga_spinlock);
    return ie;
}
static inline void vga_unlock(bool ie) {
    spin_unlock(&vga_spinlock);
    if (ie) StiHelper();
}

static void serial_init(void) {
    outb(COM1_IER, 0x00);
    outb(COM1_LCR, 0x80);
    outb(COM1_BASE, 0x03);
    outb(COM1_IER, 0x00);
    outb(COM1_LCR, 0x03);
    outb(COM1_FCR, 0xC7);
    outb(COM1_MCR, 0x0B);
}

static void serial_putc(char c) {
    if (c == '\n') {
        while ((inb(COM1_LSR) & 0x20) == 0) { }
        outb(COM1_BASE, '\r');
    }
    while ((inb(COM1_LSR) & 0x20) == 0) { }
    outb(COM1_BASE, (uint8_t)c);
}

static void serial_puts(const char* s) {
    while (*s) serial_putc(*s++);
}

void deletechar() {
    serial_puts("\b \b");
}

void internal_insert_tab() {
    for (int i = 0; i < 4; i++) serial_putc(' ');
}

void internal_putchar(char c, uint8_t color) {
    // The screen path goes through the VT parser; the parser owns the
    // current SGR state, so we must NOT slam set_attr per character or we
    // wipe out a CSI we just consumed. Callers that want a non-default color
    // wrap their output in SGR escapes (kerror does \x1b[31m...\x1b[0m).
    (void)color;
    vt_write_active_(c);

    switch (c) {
        case '\t':
            internal_insert_tab();
            return;
        case '\b':
            deletechar();
            return;
    }
    serial_putc(c);
}

void putchar(char c, uint8_t color) {
    // Route through the same path as kprintf so keyboard echo (called by
    // tty_input_byte under TTY_FLAG_ECHO) lands on both the framebuffer and
    // the serial mirror. Previously this short-circuited straight to
    // serial_putc, which is why typed keys never showed up on the VT.
    bool ie = vga_lock();
    internal_putchar(c, color);
    vga_unlock(ie);
}

uint64_t str_len(const char* str) {
    uint64_t count = 0;
    while (str[count] != '\0') count++;
    return count;
}

void internal_print_str(const char* str, uint8_t color) {
    (void)color;
    while (*str && *str != '\0') {
        internal_putchar(*str++, color);
    }
}

void print_str(const char* str, uint8_t color) {
    (void)color;
    bool ie = vga_lock();
    while (*str && *str != '\0') {
        internal_putchar(*str++, color);
    }
    vga_unlock(ie);
}

uint64_t print_str_SYSCALL(const char* str, uint8_t color, uint64_t length) {
    (void)color;
    bool ie = vga_lock();
    uint64_t idx = 0;
    while (str[idx] != '\0' && idx < length) {
        internal_putchar(str[idx], color);
        idx++;
    }
    vga_unlock(ie);
    return idx;
}

void clear_screen() {
    bool ie = vga_lock();
    serial_puts("\x1b[2J\x1b[H");
    // The VT layer handles ESC[2J via vt_write_byte; emit it as bytes here
    // so both serial and the active VT clear consistently.
    vt_write_active_(0x1B);
    vt_write_active_('[');
    vt_write_active_('2');
    vt_write_active_('J');
    vt_write_active_(0x1B);
    vt_write_active_('[');
    vt_write_active_('H');
    vga_unlock(ie);
}

void newline() {
    serial_putc('\n');
}

void insert_tab() {
    bool ie = vga_lock();
    for (uint64_t i = 0; i < 4; i++) {
        serial_putc(' ');
    }
    vga_unlock(ie);
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    bool ie = vga_lock();

    while (*format != '\0') {
        if (*format == '%') {
            format++;

            uint64_t min_width = 0;
            while (*format >= '0' && *format <= '9') {
                min_width = min_width * 10 + (*format - '0');
                format++;
            }

            char str[40];
            uint64_t num;

            switch (*format) {
            case 'c':
                internal_putchar((char)va_arg(args, uint64_t), GREY_COLOR);
                break;
            case 's':
                internal_print_str(va_arg(args, char*), GREY_COLOR);
                break;
            case 'd':
                num = va_arg(args, uint64_t);
                itoa(num, str, 10, min_width);
                internal_print_str(str, GREY_COLOR);
                break;
            case 'x':
                num = va_arg(args, uint64_t);
                str[0] = '0';
                str[1] = 'x';
                itoa(num, &str[2], 16, min_width);
                internal_print_str(str, GREY_COLOR);
                break;
            case '%':
                internal_putchar('%', GREY_COLOR);
                break;
            default:
                break;
            }
        } else {
            internal_putchar(*format, GREY_COLOR);
        }
        format++;
    }
    vga_unlock(ie);

    va_end(args);
}

void kerror(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // Panic-style force-unlock so an exception inside a kprintf still gets
    // its message out. We're about to halt anyway, so don't bother with the
    // IRQ-safe wrapper here.
    vga_spinlock.locked = 0;
    spin_lock(&vga_spinlock);
    // Bracket the message in SGR red on the screen path; the parser will
    // ignore these on the serial side too (we just send the raw escape).
    vt_write_active_(0x1B); vt_write_active_('['); vt_write_active_('3'); vt_write_active_('1'); vt_write_active_('m');
    while (*format != '\0') {
        if (*format == '%') {
            format++;

            uint64_t min_width = 0;
            while (*format >= '0' && *format <= '9') {
                min_width = min_width * 10 + (*format - '0');
                format++;
            }

            char str[20];
            uint64_t num;

            switch (*format) {
            case 'c':
                internal_putchar((char)va_arg(args, uint64_t), RED_COLOR);
                break;
            case 's':
                internal_print_str(va_arg(args, char*), RED_COLOR);
                break;
            case 'd':
                num = va_arg(args, uint64_t);
                itoa(num, str, 10, min_width);
                internal_print_str(str, RED_COLOR);
                break;
            case 'x':
                num = va_arg(args, uint64_t);
                str[0] = '0';
                str[1] = 'x';
                itoa(num, &str[2], 16, min_width);
                internal_print_str(str, RED_COLOR);
                break;
            case '%':
                internal_putchar('%', RED_COLOR);
                break;
            default:
                break;
            }
        } else {
            internal_putchar(*format, RED_COLOR);
        }
        format++;
    }

    vt_write_active_(0x1B); vt_write_active_('['); vt_write_active_('0'); vt_write_active_('m');
    va_end(args);
    spin_unlock(&vga_spinlock);
}

void InitVGA() {
    serial_init();
    clear_screen();
}
