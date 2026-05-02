[BITS 64]

section .text

global user_task_entry_trampoline
; First-run trampoline for a user task. Stack layout on entry (top -> bottom):
;   [SS=0x1B][RSP_user][RFLAGS][CS=0x23][RIP]   <- consumed by IRETQ
; Set user data segments, swap GS so KERNEL_GS_BASE retains the cpu_local
; pointer for the next syscall, then IRETQ to ring 3.
user_task_entry_trampoline:
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    swapgs
    iretq
