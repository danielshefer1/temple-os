#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

typedef struct Tuple {
    uint32_t first;
    uint32_t second;
} Tuple;

extern uint32_t write(const char* str, uint32_t length);
extern uint32_t read(const char* buffer, Tuple* triggers, uint32_t max_read);
extern uint32_t mmap(uint32_t size);
extern uint32_t munmap(void* addr);
extern uint32_t flush_consle_buffer();
extern void hlt_syscall();

void main();
void printf(const char* format, ...);
void scanf(const char *format, ...);
void flip_str(char* str);
void itoa(uint32_t value, char* str, uint32_t base, uint32_t min_width);
void memset(void* address, uint8_t value, uint32_t size);
