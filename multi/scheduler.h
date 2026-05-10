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
void task_exit(uint64_t exit_code) __attribute__((noreturn));

// Pop the first matching zombie child of `parent` from the global zombie
// list. `target_pid == 0` matches any child; otherwise the zombie's PID
// must match exactly. Returns NULL if no match.
struct task_t* zombie_list_take(struct task_t* parent, uint64_t target_pid);

// Same as zombie_list_take, but assumes the caller already holds
// zombie_list_lock and IRQs are disabled. Used by do_waitpid's
// recheck-and-park sequence so the list scan and the BLOCKED transition
// happen under the same lock the waker (task_exit) acquires.
struct task_t* zombie_list_take_locked(struct task_t* parent, uint64_t target_pid);

// Lock that serialises the zombie list and the wait/wake handshake
// between task_exit and do_waitpid.
extern spinlock_t zombie_list_lock;

// Returns 1 if `parent` has any task somewhere with t->parent == parent
// (running, ready, blocked, or zombie). Used by waitpid to short-circuit
// to -ECHILD when there's nothing to wait on.
int task_has_children(struct task_t* parent, uint64_t target_pid);

// Free a fully-dead task: closes fds, frees user address space (for tasks
// with their own PML4), releases kernel stack, kfrees the task_t. Caller
// guarantees the task is no longer running on any CPU.
void free_dead_task(struct task_t* dead);

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

// Park `t` on `cpu`'s sleep queue with `t->sleep_deadline` already set.
// Caller is responsible for setting state and calling schedule().
void sleep_queue_insert(struct cpu_local_t* cpu, struct task_t* t);

// Called from scheduler_tick on the IRQ stack. Walks this CPU's sleep queue
// head and wakes every task whose deadline has passed.
void sleep_queue_wake_expired(struct cpu_local_t* cpu);

// Find the task with the given PID across all CPUs. Walks each CPU's
// `current` and run queue. Returns NULL if no such task exists.
// Snapshot semantics — the returned task may already be exiting.
task_t* task_for_pid(uint64_t pid);

// Global doubly-linked list of every live (non-freed) task, threaded via
// task_t::all_next/all_prev. Includes BLOCKED tasks parked on driver waiter
// lists, which are not reachable through any per-CPU run queue. Consumers
// must hold tasks_lock while walking. Lock order: tasks_lock -> rq->lock.
extern task_t* tasks_head;
extern spinlock_t tasks_lock;
