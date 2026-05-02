#pragma once

#include "includes.h"
#include "types.h"

void SchedulerInit(void);

// Capture a known-good FPU/SSE state into the per-task fxstate template.
// Must be called once on the BSP after enable_sse() and before any task is
// created — fxrstor on all-zero memory faults due to reserved MXCSR bits.
void fpu_init_template(void);

// Create a kernel-space task that runs `entry` with `arg`. The task is
// allocated, its stack is set up, and it is enqueued on the ready queue.
task_t* create_kernel_task(void (*entry)(void), const char* name);

// Voluntarily yield the CPU. Caller may have IF=0 or IF=1; on return, IF
// is restored to whatever it was before the call.
void schedule(void);

// Enqueue a previously-blocked task on the run queue. Used by mutex_unlock
// and other wakeup paths.
void rq_enqueue_external(task_t* t);

// Called from the timer IRQ. Decrements current's time slice and switches
// when it reaches zero. Safe to call with interrupts already disabled.
void scheduler_tick(void);

// Terminates the current task. Marks it ZOMBIE, hands it off for reaping by
// the next schedule() call on this CPU, and switches to the next runnable
// task. Never returns. Tasks reach this implicitly when their entry function
// returns (via task_entry_trampoline) or by calling it directly.
void task_exit(void) __attribute__((noreturn));

// Bootstrap: registers the currently executing kmain/ap_kmain context as
// the per-CPU `current` task and creates a per-CPU idle task. Call once
// per CPU after cpu_init_late.
void scheduler_attach_bootstrap(const char* name);

// Implemented in switch.asm.
extern void context_switch(task_t* prev, task_t* next);
extern void task_entry_trampoline(void);

// Allocate a zeroed task with PID, fxstate template, kernel stack, and home_cpu
// filled in. Used by both create_kernel_task and create_user_task.
task_t* alloc_blank_task(const char* name);
