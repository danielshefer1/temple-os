#include "vga.h"

static uint64_t cursor_x = 0; // Column (0-79)
static uint64_t cursor_y = 0; // Row (0-24)
const uint64_t MAX_COLS = 80;
const uint64_t MAX_ROWS = 25;

static spinlock_t vga_spinlock = {0};

void deletechar() {
    if (cursor_x == 0 && cursor_y == 0) return;

    if (cursor_x == 0) {
        cursor_y--;
        cursor_x = MAX_COLS - 1;
    }
    else {
        cursor_x--;
    }
    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2] = 0x00;
}

void internal_insert_tab() {
    cursor_x += 4;
    if (cursor_x >= MAX_COLS) {
        uint64_t reminder = cursor_x % 4;
        newline();
        cursor_x = reminder;
    }
}


void internal_putchar(char c, uint8_t color) {
    switch (c) {
        case '\n':
            newline();
            return;
        case '\t':
            internal_insert_tab();
            return;
        case '\b':
            deletechar();
            return;
    }

    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2] = c;
    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2 + 1] = color;

    cursor_x++;
    if (cursor_x >= MAX_COLS) {
        newline();
    }
}


void putchar(char c, uint8_t color) {
    spin_lock(&vga_spinlock);
    switch (c) {
        case '\n':
            newline();
            spin_unlock(&vga_spinlock);
            return;
        case '\t':
            internal_insert_tab();
            spin_unlock(&vga_spinlock);
            return;
        case '\b':
            deletechar();
            spin_unlock(&vga_spinlock);
            return;
    }

    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2] = c;
    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2 + 1] = color;

    cursor_x++;
    if (cursor_x >= MAX_COLS) {
        newline();
    }
    spin_unlock(&vga_spinlock);
}

uint64_t str_len(const char* str) {
    uint64_t count = 0;
    while (str[count] != '\0') count++;
    return count;
}

void internal_print_str(const char* str, uint8_t color) {
    uint64_t len = str_len(str);
    if (cursor_x + len >= MAX_COLS) newline();

    while (*str && *str != '\0') {
        internal_putchar(*str++, color);
    }
}

void print_str(const char* str, uint8_t color) {
    spin_lock(&vga_spinlock);
    uint64_t len = str_len(str);
    if (cursor_x + len >= MAX_COLS) newline();

    while (*str && *str != '\0') {
        internal_putchar(*str++, color);
    }
    spin_unlock(&vga_spinlock);
}

uint64_t print_str_SYSCALL(const char* str, uint8_t color, uint64_t length) {
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
    for (uint64_t i = 0; i < MAX_COLS * MAX_ROWS; i++) {
        VGA_BUFFER[i * 2] = 0;         // Space character
        VGA_BUFFER[i * 2 + 1] = 0x07;    // Light gray on Black
    }
    cursor_x = 0;
    cursor_y = 0;
    spin_unlock(&vga_spinlock);
}

void newline() {
    if (cursor_y < MAX_ROWS - 1) {
        cursor_y++;
        cursor_x = 0;
    } else {
        for (uint64_t row = 1; row < MAX_ROWS; row++) {
            for (uint64_t col = 0; col < MAX_COLS; col++) {
                VGA_BUFFER[((row - 1) * MAX_COLS + col) * 2] = 
                    VGA_BUFFER[(row * MAX_COLS + col) * 2];
                VGA_BUFFER[((row - 1) * MAX_COLS + col) * 2 + 1] = 
                    VGA_BUFFER[(row * MAX_COLS + col) * 2 + 1];
            }
        }
        // Clear the last line
        for (uint64_t col = 0; col < MAX_COLS; col++) {
            VGA_BUFFER[((MAX_ROWS - 1) * MAX_COLS + col) * 2] = ' ';
            VGA_BUFFER[((MAX_ROWS - 1) * MAX_COLS + col) * 2 + 1] = 0x07;
        }
        cursor_x = 0;
    }
}



void insert_tab() {
    spin_lock(&vga_spinlock);
    for (uint64_t i = 0; i < 4; i++) {
        internal_putchar(' ', GREY_COLOR);
    }
    spin_unlock(&vga_spinlock);

}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    spin_lock(&vga_spinlock);
    
    if (cursor_x + str_len(format) >= MAX_COLS) newline();
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            // Check for width specifier FIRST
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
    if (cursor_x + str_len(format) >= MAX_COLS) newline();
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            // Check for width specifier FIRST
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
    clear_screen();
}