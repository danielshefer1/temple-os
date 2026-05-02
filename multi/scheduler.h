#pragma once

#include "includes.h"
#include "types.h"

void SchedulerInit(void);

// Create a kernel-space task that runs `entry` with `arg`. The task is
// allocated, its stack is set up, and it is enqueued on the ready queue.
task_t* create_kernel_task(void (*entry)(void), const char* name);

// Voluntarily yield the CPU. Caller may have IF=0 or IF=1; on return, IF
// is restored to whatever it was before the call.
void schedule(void);

// Called from the timer IRQ. Decrements current's time slice and switches
// when it reaches zero. Safe to call with interrupts already disabled.
void scheduler_tick(void);

// Bootstrap: registers the currently executing kmain/ap_kmain context as
// the per-CPU `current` task and creates a per-CPU idle task. Call once
// per CPU after cpu_init_late.
void scheduler_attach_bootstrap(const char* name);

// Implemented in switch.asm.
extern void context_switch(task_t* prev, task_t* next);
extern void task_entry_trampoline(void);
