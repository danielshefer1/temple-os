#pragma once

#include "includes.h"
#include "types.h"
#include "elf64.h"

// Build a runnable task that returns to ring 3 at img->entry with RSP=img->stack_top
// and CR3=img->cr3_phys, and enqueue it on its home CPU's run queue.
// Returns the task or NULL on allocation failure.
task_t* create_user_task(const elf64_image_t* img, const char* name);

// Implemented in multi/user_trampoline.asm. Restores DS/ES, SWAPGS, IRETQ to ring 3.
extern void user_task_entry_trampoline(void);
