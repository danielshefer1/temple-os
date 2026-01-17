#pragma once
#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "str_ops.h"

void deletechar();
void putchar(char c, uint8_t color);
void print_str(const char* str, uint8_t color);
uint32_t print_str_SYSCALL(const char* str, uint8_t color, uint32_t length);
void newline();
void insert_tab();
void clear_screen();
void putchar(char c, uint8_t color);
void kprintf(const char* format, ...);
void kerror(const char* format, ...);
void InitVGA();