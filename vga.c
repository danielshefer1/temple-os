#include "vga.h"

static uint32_t cursor_x = 0; // Column (0-79)
static uint32_t cursor_y = 0; // Row (0-24)
const uint32_t MAX_COLS = 80;
const uint32_t MAX_ROWS = 25;

void deletechar() {
    if (cursor_x == 0 && cursor_y == 0) return;

    if (cursor_x == 0) {
        cursor_y--;
        cursor_x = MAX_COLS - 1;
    }
    else {
        cursor_x--;
    }
    VGA_BUFFER[(cursor_y * MAX_COLS + cursor_x) * 2] = 0x00;}

void putchar(char c, uint8_t color) {
    switch (c) {
        case '\n':
            newline();
            return;
        case '\t':
            insert_tab();
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

uint32_t str_len(const char* str) {
    uint32_t count = 0;
    while (str[count] != '\0') count++;
    return count;
}

void print_str(const char* str, uint8_t color) {
    uint32_t len = str_len(str);
    if (cursor_x + len >= MAX_COLS) newline();

    while (*str && *str != '\0') {
        putchar(*str++, color);
    }
}

void clear_screen() {
    for (uint32_t i = 0; i < MAX_COLS * MAX_ROWS; i++) {
        VGA_BUFFER[i * 2] = 0;         // Space character
        VGA_BUFFER[i * 2 + 1] = 0x07;    // Light gray on Black
    }
    cursor_x = 0;
    cursor_y = 0;
}

void newline() {
    if (cursor_y < MAX_ROWS - 1) {
        cursor_y++;
        cursor_x = 0;
    } else {
        for (uint32_t row = 1; row < MAX_ROWS; row++) {
            for (uint32_t col = 0; col < MAX_COLS; col++) {
                VGA_BUFFER[((row - 1) * MAX_COLS + col) * 2] = 
                    VGA_BUFFER[(row * MAX_COLS + col) * 2];
                VGA_BUFFER[((row - 1) * MAX_COLS + col) * 2 + 1] = 
                    VGA_BUFFER[(row * MAX_COLS + col) * 2 + 1];
            }
        }
        // Clear the last line
        for (uint32_t col = 0; col < MAX_COLS; col++) {
            VGA_BUFFER[((MAX_ROWS - 1) * MAX_COLS + col) * 2] = ' ';
            VGA_BUFFER[((MAX_ROWS - 1) * MAX_COLS + col) * 2 + 1] = 0x07;
        }
        cursor_x = 0;
    }
}

void insert_tab() {
    for (uint32_t i = 0; i < 4; i++) {
        putchar(' ', GREY_COLOR);
    }
}

void kprintf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (cursor_x + str_len(format) >= MAX_COLS) newline();
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            // Check for width specifier FIRST
            uint32_t min_width = 0;
            while (*format >= '0' && *format <= '9') {
                min_width = min_width * 10 + (*format - '0');
                format++;
            }
            
            char str[20];
            uint32_t num;
            
            switch (*format) {
            case 'c':
                putchar((char)va_arg(args, uint32_t), GREY_COLOR);
                break;
            case 's':
                print_str(va_arg(args, char*), GREY_COLOR);
                break;
            case 'd':
                num = va_arg(args, uint32_t);
                itoa(num, str, 10, min_width);
                print_str(str, GREY_COLOR);
                break;
            case 'x':
                num = va_arg(args, uint32_t);
                str[0] = '0';
                str[1] = 'x';
                itoa(num, &str[2], 16, min_width);
                print_str(str, GREY_COLOR);
                break;
            case '%':
                putchar('%', GREY_COLOR);
                break;
            default:
                break;
            }
        } else {
            putchar(*format, GREY_COLOR);
        }
        format++;
    }
    
    va_end(args);
}

void kerror(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    if (cursor_x + str_len(format) >= MAX_COLS) newline();
    while (*format != '\0') {
        if (*format == '%') {
            format++;
            
            // Check for width specifier FIRST
            uint32_t min_width = 0;
            while (*format >= '0' && *format <= '9') {
                min_width = min_width * 10 + (*format - '0');
                format++;
            }
            
            char str[20];
            uint32_t num;
            
            switch (*format) {
            case 'c':
                putchar((char)va_arg(args, uint32_t), RED_COLOR);
                break;
            case 's':
                print_str(va_arg(args, char*), RED_COLOR);
                break;
            case 'd':
                num = va_arg(args, uint32_t);
                itoa(num, str, 10, min_width);
                print_str(str, RED_COLOR);
                break;
            case 'x':
                num = va_arg(args, uint32_t);
                str[0] = '0';
                str[1] = 'x';
                itoa(num, &str[2], 16, min_width);
                print_str(str, RED_COLOR);
                break;
            case '%':
                putchar('%', RED_COLOR);
                break;
            default:
                break;
            }
        } else {
            putchar(*format, RED_COLOR);
        }
        format++;
    }
    
    va_end(args);

    CliHelper();
    HltHelper();
}

void InitVGA() {
    clear_screen();
}