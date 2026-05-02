; SYSCALL entry stub.
;
; On entry from user mode:
;   RCX = user RIP, R11 = user RFLAGS, IF/DF cleared by FMASK.
;   CS = 0x08 (kernel), SS = 0x10 (kernel), but RSP still user.
;   GS_BASE = user value (whatever userspace had), KERNEL_GS_BASE = &cpu_locals[idx];
;   SWAPGS below brings the per-CPU pointer into GS for the syscall window.
;
; ABI (Linux convention): syscall args in rax (number), rbx, r10, rdx, rsi, rdi, r8, r9.
; R10 takes the place of RCX because RCX is clobbered by SYSCALL.

extern syscall_handler

; cpu_local_t field offsets — keep in sync with util/types.h
%define CPU_LOCAL_KERNEL_RSP   8
%define CPU_LOCAL_SCRATCH_RSP  16

; Saved-RCX offset in the synthesized interrupt_frame_t (after PUSHAQ + push fs + push gs).
; Layout from low to high: gs(0), fs(8), r15..r8 (16..72), rdi(80), rsi(88), rbp(96),
;   rsp_dummy(104), rbx(112), rdx(120), rcx(128), rax(136), int_no(144), err_code(152),
;   rip(160), cs(168), rflags(176), userrsp(184), ss(192).
%define FRAME_RCX_OFFSET   128

%macro PUSHAQ 0
    push rax
    push rcx
    push rdx
    push rbx
    sub rsp, 8          ; struct rsp slot (placeholder; gap matches interrupt_frame_t)
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POPAQ 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    add rsp, 8          ; skip struct rsp slot
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro

global syscall_entry
syscall_entry:
    swapgs                                  ; GS now -> cpu_local
    mov [gs:CPU_LOCAL_SCRATCH_RSP], rsp     ; stash user RSP
    mov rsp, [gs:CPU_LOCAL_KERNEL_RSP]      ; switch to kernel stack

    ; Build an interrupt_frame_t-shaped frame.
    ; Top of frame (high addresses) first.
    push qword 0x1B                         ; ss
    push qword [gs:CPU_LOCAL_SCRATCH_RSP]   ; userrsp
    push r11                                ; rflags (saved by SYSCALL)
    push qword 0x23                         ; cs
    push rcx                                ; rip (saved by SYSCALL)
    push qword 0                            ; err_code
    push qword 0                            ; int_no

    PUSHAQ
    push fs
    push gs

    ; Patch saved-rcx with R10 so existing C handlers (which read frame->rcx as
    ; the syscall's third-position argument) keep working unchanged. R10 still
    ; holds the user's value because we have not touched it.
    mov [rsp + FRAME_RCX_OFFSET], r10

    cld                                     ; SysV ABI: DF clear (FMASK already did it)
    mov rdi, rsp
    call syscall_handler

    add rsp, 16                             ; skip saved gs, fs (long mode: don't touch segregs)
    POPAQ
    add rsp, 16                             ; drop int_no + err_code

    ; Stack now holds: rip, cs, rflags, userrsp, ss
    pop rcx                                 ; user RIP -> RCX (sysretq target)
    add rsp, 8                              ; skip cs
    pop r11                                 ; user RFLAGS -> R11
    pop rsp                                 ; user RSP (skipping ss is fine, we switch stacks)
    swapgs
    o64 sysret                              ; REX.W sysretq
