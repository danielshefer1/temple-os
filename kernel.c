#include <stdint.h>
#include <stdarg.h>

static int cursor_x = 0; // Column (0-79)
static int cursor_y = 0; // Row (0-24)
const int MAX_COLS = 80;
const int MAX_ROWS = 25;
volatile char* VGA_BUFFER = (volatile char*)0xB8000;

void newline();
void insert_tab();
void clear_screen();
void putchar(char c);
void print_str(const char* str);
void kprintf(const char* format, ...);


void itoa(int value, char* str, int base) {
    char* ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789ABCDEF"[tmp_value - value * base];
    } while (value);

    *ptr-- = '\0';

    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
}

void putchar(char c) {
    // 0xB8000 is the standard VGA text buffer address
    volatile char* VGA_BUFFER = (volatile char*)0xB8000;

    if (c == '\n') {
        newline();
        return;
    } else if (c == '\t') {
        insert_tab();
        return;
    }

    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2] = c;
    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2 + 1] = 0x07; // Light grey on black background

    cursor_x++;
    if (cursor_x >= MAX_COLS) {
        newline();
    }
}

void print_str(const char* str) {
    while (*str || *str != '\0') {
        putchar(*str++);
    }
}

void clear_screen() {
    for (int i = 0; i < 80 * 25 * 2; i++) {
        VGA_BUFFER[i] = 0;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void newline() {
    // Move cursor to the beginning of the next line
    // This is a simplified version and does not handle scrolling

    if (cursor_y < MAX_ROWS - 1) {
        cursor_y++;
        cursor_x = 0;
    } else {
        clear_screen();
        cursor_y = 0;
        cursor_x = 0;
    }
}

void insert_tab() {
    for (int i = 0; i < 4; i++) {
        putchar(' ');
    }
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    while (*format || *format != '\0') {
        if (*format == '%') {
            format++;
            if (*format == 'c') {
                char c = (char)va_arg(args, int);
                putchar(c);
            } else if (*format == 's') {
                char* str = va_arg(args, char*);
                print_str(str);
            } else if (*format == 'd') {
                int num = va_arg(args, int);
                char str[20];
                itoa(num, str, 10);
                print_str(str);
            } else if (*format == 'x') {
                int num = va_arg(args, int);
                char str[20];
                itoa(num, str, 16);
                print_str("0x");
                print_str(str);
            }
        } else {
            putchar(*format);
        }
        format++;
    }
    
    va_end(args);
}

void kmain(void) {
    clear_screen();

    kprintf("Hello, Kernel World!\n");
    kprintf("Decimal: %d\t", 12345);
    kprintf("Hexadecimal: %x\n", 0xABCD);
}