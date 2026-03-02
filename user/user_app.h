#pragma once

#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

typedef struct tuple_t {
    uint64_t first;
    uint64_t second;
} tuple_t;

extern uint64_t write(const char* str, uint64_t length);
extern uint64_t read(const char* buffer, tuple_t* triggers, uint64_t max_read);
extern uint64_t mmap(uint64_t size);
extern uint64_t munmap(void* addr);
extern uint64_t flush_consle_buffer();
extern void exit();

void main();
void printf(const char* format, ...);
void scanf(const char *format, ...);
void flip_str(char* str);
void itoa(uint64_t value, char* str, uint64_t base, uint64_t min_width);
void memset(void* address, uint8_t value, uint64_t size);
