#include <stdint.h>
#include <stdarg.h>

static uint32_t cursor_x = 0; // Column (0-79)
static uint32_t cursor_y = 0; // Row (0-24)
const uint32_t MAX_COLS = 80;
const uint32_t MAX_ROWS = 25;
#define VGA_BUFFER ((volatile char*)0xB8000)

void flip_str(char* str);
void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width);
void newline();
void insert_tab();
void clear_screen();
void putchar(char c);
void pruint32_t_str(const char* str);
void kprintf(const char* format, ...);


void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width) {
    char* ptr = str;
    uint32_t tmp_value, count = 0;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return;
    }

    do {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789ABCDEF"[tmp_value - value * base];
        count++;
    } while (value);

    while (count++ < min_width) {
        *ptr++ = '0';
    }

    *ptr-- = '\0';

    flip_str(str);
}

void flip_str(char* str) {
    char* start = str;
    char* end = str;

    while (*end != '\0') {
        end++;
    }
    end--;

    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
}

void putchar(char c) {
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

void pruint32_t_str(const char* str) {
    while (*str || *str != '\0') {
        putchar(*str++);
    }
}

void clear_screen() {
    for (uint32_t i = 0; i < 80 * 25 * 2; i++) {
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
    for (uint32_t i = 0; i < 4; i++) {
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
                char c = (char)va_arg(args, uint32_t);
                putchar(c);
            } else if (*format == 's') {
                char* str = va_arg(args, char*);
                pruint32_t_str(str);
            } else if (*format == 'd') {
                uint32_t num = va_arg(args, uint32_t);
                char str[20];
                itoa(num, str, 10, 0);
                pruint32_t_str(str);
            } else if (*format == 'x') {
                uint32_t num = va_arg(args, uint32_t);
                char str[20];
                itoa(num, str, 16, 0);
                kprintf("0x%s", str);
            } else if (*format == '%') {
                putchar('%');
            }
            else if (*format <= '9' && *format >= '0') {
                uint32_t min_width = 0;
                while (*format <= '9' && *format >= '0') {
                    min_width = min_width * 10 + (*format - '0');
                    format++;
                }
                if (*format == 'd') {
                    uint32_t num = va_arg(args, uint32_t);
                    char str[20];
                    itoa(num, str, 10, min_width);
                    pruint32_t_str(str);
                } else if (*format == 'x') {
                    uint32_t num = va_arg(args, uint32_t);
                    char str[20];
                    itoa(num, str, 16, min_width);
                    kprintf("0x%s", str);
                }
            }
        } else {
            putchar(*format);
        }
        format++;
    }
    
    va_end(args);
}

void kmain() {
    clear_screen();

    kprintf("Hello, Kernel World!\n");
    kprintf("Decimal: %d\t", 12345);
    kprintf("Hexadecimal: %x\n", 0xABCD);
    kprintf("Character: %c\t", 'A');
    kprintf("String: %s\n", "Test String");
    kprintf("Percent Sign: %%\n");
    kprintf("Padded Number: %05d\t", 42);
    kprintf("Padded Hex: %08x\n", 0x1A3);
}