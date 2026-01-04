#pragma once

#include <stdint.h>
#include "print_text.h"
#include "paging_bootstrap.h"
#include "E820.h"
#include "buddy_alloc.h"
#include "paging.h"

#define PAGE_SIZE 4096

void kmain();