#pragma once
#include <stdint.h>
#include <stdarg.h>

#define KERNEL_VIRTUAL 0x80000000
#define VGA_BUFFER ((volatile char*) 0xB8000)

void print_str(const char* str);
void flip_str(char* str);
void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width);
void newline();
void insert_tab();
void clear_screen();
void putchar(char c);
void pruint32_t_str(const char* str);
void kprintf(const char* format, ...);