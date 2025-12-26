#pragma once

#include <stdint.h>
#include "print_text.h"
#include "paging.h"

extern uint32_t __total_pages;

#define PAGE_SIZE 4096

void kmain();