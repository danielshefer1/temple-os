extern isr_handler
extern syscall_handler
extern irq_handler
extern lapic
extern __total_pages
extern kmain
extern _stack_top

section .helpers

global CliHelper
CliHelper:
    cli
    ret

global HltHelper
HltHelper:
    hlt
    ret

global StiHelper
StiHelper:
    sti
    ret

global PauseHelper
PauseHelper:
    pause
    ret

global LoadGDTHelper
LoadGDTHelper:
    lgdt [rdi]
    push 0x08
    lea rax, [rel flush_cs]
    push rax
    retfq

flush_cs:
    mov ax, 0x10       ; load kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret 

global LoadIDTHelper
LoadIDTHelper:
    lidt [rdi]
    ret

global inb
inb:
    mov dx, di
    in al, dx
    ret

global inw
inw:
    mov dx, di
    in ax, dx
    ret

global inl
inl:
    mov dx, di
    in eax, dx
    ret

global outb
outb:
    mov dx, di
    mov ax, si
    out dx, al
    ret

global outw
outw:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

global outl
outl:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

global check_interrupts
check_interrupts:
    pushfq
    pop rax
    shr rax, 9
    and rax, 1
    ret

global LoadTSS
LoadTSS:
    mov ax, di
    ltr ax
    ret

global rdmsr
rdmsr:
    mov ecx, edi
    rdmsr
    shl rdx, 32
    or  rax, rdx
    ret

global wrmsr
wrmsr:
    mov ecx, edi
    mov rax, rsi
    mov rdx, rsi
    shr rdx, 32
    wrmsr
    ret

extern init_gs_and_get_cpuid

global get_cpuid
get_cpuid:
    ; Fast path: IA32_GS_BASE points at this CPU's cpu_local_t. Read the
    ; cached apic_id at offset 28. If GS_BASE is still 0 (early BSP, AP
    ; entry pre-cpu_init_late), fall through to the C slow path which does
    ; CPUID + wrmsr and returns the apic_id.
    mov ecx, 0xC0000101         ; IA32_GS_BASE
    rdmsr                       ; edx:eax = MSR; clobbers rcx/rdx (caller-saved)
    or eax, edx
    jz .uninit
    movzx rax, byte [gs:28]     ; cpu_local_t.apic_id (offset 28, low byte)
    ret
.uninit:
    jmp init_gs_and_get_cpuid   ; tail call

global enable_sse
enable_sse:
    mov rax, cr0
    and ax, 0xFFFB      ; Clear EM bit
    or ax, 0x2          ; Set MP bit
    mov cr0, rax

    ; 2. CR4: Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    mov rax, cr4
    or ax, (3 << 9)     ; Set bits 9 and 10
    mov cr4, rax
    ret

global spin_lock
spin_lock:    
    mov rax, 1              

.retry:
    xchg rax, [rdi]      
    test rax, rax           
    jnz .pause_and_retry   
    ret                     

.pause_and_retry:
    pause                   
    jmp .retry


global spin_unlock
spin_unlock:
    xor eax, eax
    mov [rdi], rax          ; 64-bit release store; aligned -> atomic on x86
    ret

global switch_pml4
switch_pml4:
    mov cr3, rdi
    mov rax, [rsp]
    ret

global flush_tlb
flush_tlb:
    mov rax, cr3
    mov cr3, rax
    ret

global InvlpgHelper
InvlpgHelper:
    invlpg [rdi]
    ret

; Macro to create ISR stub without error code
%macro ISR_STUB_NO_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push 0
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_STUB_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push %1
    jmp isr_common_stub
%endmacro

%macro ISR_PIC_STUB 1
global isr_pic_stub_%1
isr_pic_stub_%1:
    push 0
    push %1

    jmp isr_pic_stub
%endmacro

%macro ISR_APIC_STUB 1
global isr_apic_stub_%1
isr_apic_stub_%1:
    push 0
    push %1
    jmp isr_apic_stub
%endmacro

global isr_spurious
isr_spurious:
    iret

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

ISR_STUB_NO_ERROR 0
ISR_STUB_NO_ERROR 1
ISR_STUB_NO_ERROR 2
ISR_STUB_NO_ERROR 3
ISR_STUB_NO_ERROR 4
ISR_STUB_NO_ERROR 5
ISR_STUB_NO_ERROR 6
ISR_STUB_NO_ERROR 7
ISR_STUB_ERROR 8
ISR_STUB_NO_ERROR 9
ISR_STUB_ERROR 10
ISR_STUB_ERROR 11
ISR_STUB_ERROR 12
ISR_STUB_ERROR 13
ISR_STUB_ERROR 14
ISR_STUB_NO_ERROR 16
ISR_STUB_ERROR 17
ISR_STUB_NO_ERROR 18
ISR_STUB_NO_ERROR 19
ISR_STUB_NO_ERROR 20
ISR_STUB_ERROR 21
ISR_PIC_STUB 32
ISR_APIC_STUB 32
ISR_APIC_STUB 33
ISR_APIC_STUB 64
ISR_APIC_STUB 65


global isr_common_stub
isr_common_stub:
    ; If we entered from user mode (CS RPL != 0), GS_BASE currently holds
    ; user-controlled state. swapgs brings the per-CPU pointer (saved in
    ; KERNEL_GS_BASE) into GS_BASE so the kernel handler can use gs:[off]
    ; safely. We only swap on user->kernel transitions, so kernel->kernel
    ; IRQs leave GS_BASE alone.
    test byte [rsp + 24], 3        ; CS RPL in the iret frame
    jz .from_kernel_in
    swapgs
.from_kernel_in:
    PUSHAQ             ; save all registers
    ; Reserve the fs/gs slots in interrupt_frame_t. We do NOT push the
    ; segment registers themselves: in long mode `mov gs, ax`/`pop gs`
    ; reload GS_BASE from the GDT, which clobbers the per-CPU pointer
    ; this kernel keeps in IA32_GS_BASE.
    push qword 0       ; fs slot
    push qword 0       ; gs slot

    mov rdi, rsp
    call isr_handler

    add rsp, 16         ; skip fs/gs placeholder slots
    POPAQ
    add rsp, 16         ; clean up int num and error code
    test byte [rsp + 8], 3
    jz .from_kernel_out
    swapgs
.from_kernel_out:
    iretq

global isr_pic_stub
isr_pic_stub:
    test byte [rsp + 24], 3
    jz .from_kernel_in
    swapgs
.from_kernel_in:
    PUSHAQ
    push qword 0       ; fs slot
    push qword 0       ; gs slot

    mov rdi, rsp
    call isr_handler

    mov al, 0x20
    out 0xA0, al      ; slave PIC
    out 0x20, al      ; master PIC

    add rsp, 16         ; skip fs/gs placeholder slots
    POPAQ

    add rsp, 16

    test byte [rsp + 8], 3
    jz .from_kernel_out
    swapgs
.from_kernel_out:
    iretq

global isr_apic_stub
isr_apic_stub:
    test byte [rsp + 24], 3
    jz .from_kernel_in
    swapgs
.from_kernel_in:
    PUSHAQ
    push qword 0       ; fs slot
    push qword 0       ; gs slot

    mov rdi, rsp
    call irq_handler

    add rsp, 16         ; skip fs/gs placeholder slots
    POPAQ

    add rsp, 16

    test byte [rsp + 8], 3
    jz .from_kernel_out
    swapgs
.from_kernel_out:
    iretq

