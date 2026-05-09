#pragma once
#include "includes.h"
#include "lock_types.h"
#include "gdt_types.h"
#include "syscall_defs.h"
#include "signal_defs.h"

// Forward declaration so we can hold pointers to file_t without dragging
// the full vfs_types.h into the task struct (vfs_types.h is included
// later in util/types.h after this header).
struct file_t;

typedef struct fd_entry_t {
    struct file_t* file;
    uint32_t flags;
} fd_entry_t;

typedef struct sigaction_t {
    void* handler;     // SIG_DFL, SIG_IGN, or a user-space function pointer
    void* restorer;    // user-space trampoline that calls SYS_SIGRETURN
} sigaction_t;

typedef struct run_queue_t {
    struct task_t* head;
    struct task_t* tail;
    spinlock_t lock;
} run_queue_t;

typedef struct cpu_local_t {
    uint64_t self;              // offset 0  — &cpu_locals[i] (gs:[0])
    uint64_t kernel_rsp;        // offset 8  — top of kernel stack (== tss.rsp0)
    uint64_t scratch_user_rsp;  // offset 16 — syscall_entry saves user RSP here
    uint32_t cpu_index;         // offset 24
    uint32_t apic_id;           // offset 28
    tss64_t* tss;               // offset 32
    uint64_t kstack_top;        // offset 40
    struct task_t* current;     // offset 48 — currently running task on this CPU
    struct task_t* pending_reap;// offset 56 — zombie left behind by previous switch
    run_queue_t rq;             // this CPU's runnable tasks
    struct task_t* sleep_head;  // sorted ascending by sleep_deadline
    spinlock_t sleep_lock;
} __attribute__((packed)) cpu_local_t;

// ---- Multi-tasking ----
// Offsets used by multi/switch.asm. If you reorder task_t, update these.
#define TASK_OFF_SAVED_RSP   0
#define TASK_OFF_CR3         8
#define TASK_OFF_KSTACK_TOP  16
#define TASK_OFF_FXSTATE     128
#define CPU_LOCAL_OFF_SELF    0
#define CPU_LOCAL_OFF_KRSP   8
#define CPU_LOCAL_OFF_APIC_ID 28
#define CPU_LOCAL_OFF_TSS    32
#define CPU_LOCAL_OFF_CURRENT 48
#define TSS_OFF_RSP0         4

typedef enum {
    TASK_STATE_NEW = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_ZOMBIE,
} task_state_t;

typedef struct task_t {
    uint64_t saved_rsp;        // offset 0  — kernel RSP at last context switch
    uint64_t cr3;              // offset 8  — physical address of this task's PML4
    uint64_t kstack_top;       // offset 16 — top of kernel stack (used for tss.rsp0)
    uint64_t kstack_base;      // offset 24 — base (low) of kernel stack allocation
    uint64_t kstack_pages;     // number of pages allocated for kernel stack
    uint64_t pid;
    task_state_t state;
    uint64_t time_slice;       // ticks remaining in this quantum
    struct task_t* next;       // run-queue link
    struct task_t* prev;
    char name[32];
    uint32_t home_cpu;         // CPU index whose run queue owns this task
    uint8_t fxstate[512] __attribute__((aligned(16)));
    fd_entry_t fds[FD_MAX];    // per-task open file table (zeroed on alloc)
    uint64_t pending_signals;  // bit (signo - 1) set => signal pending
    sigaction_t signal_actions[NSIG];

    // Process tree / wait support.
    struct task_t* parent;     // task that fork()ed this one; NULL for orphans/kernel
    uint64_t wait_target;      // when state==BLOCKED for waitpid: 0=any child, else PID
    uint64_t exit_code;        // set by task_exit; readable by waitpid
    struct task_t* zombie_next;// link in the global zombie list

    uint64_t sleep_deadline;   // tick value at which to wake (0 if not sleeping)
    struct task_t* sleep_next; // per-CPU sleep queue link (sorted ascending)

    uint64_t mmap_next;        // next user VA for anonymous mmap (bump-only)

    // Process group / session model. New tasks default to a fresh "session
    // leader" (pgid == sid == pid); fork inherits both from the parent;
    // exec preserves them. tty's foreground pgrp uses the pgid value.
    uint64_t pgid;
    uint64_t sid;
} task_t;
