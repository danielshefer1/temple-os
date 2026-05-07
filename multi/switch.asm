; void context_switch(task_t* prev, task_t* next)
;   prev = rdi, next = rsi
;
; Saves callee-saved regs on prev's kernel stack, swaps RSP via prev->saved_rsp
; / next->saved_rsp, conditionally reloads CR3, returns into next's prior
; context_switch return site (or task_entry_trampoline for a brand-new task).
;
; Per-CPU state (cpu_local->current, cpu_local->tss->rsp0) is updated by the
; C scheduler BEFORE calling here; this routine only swaps register state and
; stacks, it does not touch the per-CPU struct.

%define TASK_OFF_SAVED_RSP   0
%define TASK_OFF_CR3         8
%define TASK_OFF_FXSTATE     128

section .text

global context_switch
context_switch:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    fxsave64 [rdi + TASK_OFF_FXSTATE]

    mov [rdi + TASK_OFF_SAVED_RSP], rsp
    mov rsp, [rsi + TASK_OFF_SAVED_RSP]

    fxrstor64 [rsi + TASK_OFF_FXSTATE]

    mov rax, [rdi + TASK_OFF_CR3]
    mov rcx, [rsi + TASK_OFF_CR3]
    cmp rax, rcx
    je .no_cr3
    mov cr3, rcx
.no_cr3:

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

; Trampoline for brand-new tasks. create_kernel_task seeds the stack so that
; r12 = entry_fn and the return address from context_switch is this label.
; When the entry function returns naturally, fall into task_exit so the task
; is reaped by the scheduler. task_exit is __attribute__((noreturn)); the
; halt loop is a defensive backstop.
extern task_exit

global task_entry_trampoline
task_entry_trampoline:
    sti
    call r12
    xor edi, edi          ; exit_code = 0 when entry function returns naturally
    call task_exit
.halt:
    cli
    hlt
    jmp .halt
