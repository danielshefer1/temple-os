#pragma once
#include "includes.h"
#include "types.h"
#include "defintions.h"

void print_str(const char* str, uint8_t color);
void flip_str(char* str);
void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width);
void newline();
void insert_tab();
void clear_screen();
void putchar(char c, uint8_t color);
void kprintf(const char* format, ...);
void kerror(const char* format, ...);