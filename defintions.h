#pragma once
#include "includes.h"

#define NUM_CACHE 1
#define VGA_BUFFER ((volatile char*)(uint32_t) KERNEL_VIRTUAL + 0xB8000)
#define PAGE_SIZE 4096
#define KERNEL_VIRTUAL 0x80000000
#define KERNEL_BASE 0x100000
#define TABLE_SIZE PAGE_SIZE * 1024
#define HIGHER_HALF_IDX 512
#define E820_SIGNATURE 0x534D4150
#define E820_ADDRESS KERNEL_VIRTUAL + 0x500
#define MAX_ORDER 32