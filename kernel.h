#pragma once

#include <stdint.h>
#include "print_text.h"
#include "paging_bootstrap.h"
#include "E820.h"
#include "buddy_alloc.h"

extern uint32_t __total_pages;

#define PAGE_SIZE 4096

void kmain();