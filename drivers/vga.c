#include "vga.h"
#include "console.h"

#define COM1_IER (COM1_BASE + 1)
#define COM1_LCR (COM1_BASE + 3)
#define COM1_MCR (COM1_BASE + 4)

static spinlock_t vga_spinlock = {0};

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
    // Mirror to the screen console (no-op until M3 plugs in fb_console). The
    // serial port is the source of truth for kernel logs; the screen is a
    // best-effort visual mirror.
    console_set_attr(color & 0x0F, (color >> 4) & 0x0F);
    console_putc(c);

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
    (void)color;
    spin_lock(&vga_spinlock);
    switch (c) {
        case '\t':
            internal_insert_tab();
            spin_unlock(&vga_spinlock);
            return;
        case '\b':
            deletechar();
            spin_unlock(&vga_spinlock);
            return;
    }
    serial_putc(c);
    spin_unlock(&vga_spinlock);
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
    spin_lock(&vga_spinlock);
    while (*str && *str != '\0') {
        internal_putchar(*str++, color);
    }
    spin_unlock(&vga_spinlock);
}

uint64_t print_str_SYSCALL(const char* str, uint8_t color, uint64_t length) {
    (void)color;
    spin_lock(&vga_spinlock);
    uint64_t idx = 0;
    while (str[idx] != '\0' && idx < length) {
        internal_putchar(str[idx], color);
        idx++;
    }
    spin_unlock(&vga_spinlock);
    return idx;
}

void clear_screen() {
    spin_lock(&vga_spinlock);
    serial_puts("\x1b[2J\x1b[H");
    console_clear();
    spin_unlock(&vga_spinlock);
}

void newline() {
    serial_putc('\n');
}

void insert_tab() {
    spin_lock(&vga_spinlock);
    for (uint64_t i = 0; i < 4; i++) {
        serial_putc(' ');
    }
    spin_unlock(&vga_spinlock);
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    spin_lock(&vga_spinlock);

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
    spin_unlock(&vga_spinlock);

    va_end(args);
}

void kerror(const char* format, ...) {
    va_list args;
    va_start(args, format);

    vga_spinlock.locked = 0;
    spin_lock(&vga_spinlock);
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

    va_end(args);
    spin_unlock(&vga_spinlock);
}

void InitVGA() {
    serial_init();
    clear_screen();
}
