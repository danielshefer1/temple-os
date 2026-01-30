[BITS 32]

extern isr_handler
extern syscall_handler
extern irq_handler
extern lapic

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
    mov eax, [esp + 4] ; pointer to GDTR
    lgdt [eax]
    jmp 0x08:flush_cs

flush_cs:
    mov eax, 0x10       ; load kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global LoadIDTHelper
LoadIDTHelper:
    mov eax, [esp + 4] ; pointer to IDTR
    lidt [eax]
    ret

global inb
inb:
    push ebp
    mov ebp, esp
    xor eax, eax
    mov dx, [ebp + 8]   ; Port
    in al, dx
    pop ebp
    ret

global outb
outb:
    push ebp
    mov ebp, esp
    mov dx, [ebp + 8]   ; Port
    mov eax, [ebp + 12] ; Data
    out dx, al
    pop ebp
    ret

global check_interrupts
check_interrupts:
    pushfd              ; Push EFLAGS (32-bit) onto the stack
    pop eax             ; Pop EFLAGS into EAX
    shr eax, 9          ; Shift right by 9 to put the 'IF' bit at position 0
    and eax, 1          ; Mask everything else (get only the IF bit)
    ret

global load_tss
load_tss:
    xor eax, eax
    mov ax, 0x2B      ; Index 5 | RPL 3 (0x28 | 3 = 0x2B)
    ltr ax            ; Load Task Register
    ret

global switch_to_user_mode
switch_to_user_mode:
    mov ax, 0x23      ; User Data Selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebx, [esp + 4] ; EIP from the stack
    mov ecx, [esp + 8] ; ESP from the stack

    ; [SS] [ESP] [EFLAGS] [CS] [EIP]
    
    push 0x23 ; User Stack Segment 
    push ecx ; ESP
    
    pushfd ; Push current EFLAGS
    pop eax
    or eax, 0x200 ; Set IF bit (Interrupt Flag) so interrupts are enabled
    push eax
    
    push 0x1B ; User Code Segment
    push ebx ; EIP

    iret ; Pop registers and drop to Ring 3

global get_cpuid
get_cpuid:
    mov eax, 1
    cpuid
    shr ebx, 24
    mov eax, ebx
    ret

global enable_sse
enable_sse:
    ; --- CR0 Setup ---
    mov eax, cr0
    and eax, ~0x4       ; Clear EM (bit 2)
    or eax, 0x22        ; Set MP (bit 1) and NE (bit 5)
    mov cr0, eax

    ; --- CR4 Setup ---
    mov eax, cr4
    or eax, 0x600       ; 0x600 sets bits 9 and 10
    mov cr4, eax

    ret

global spin_lock
spin_lock:
    mov edx, [esp + 4]      ; Get the address of the lock
    mov eax, 1              ; We want to set the lock to 1

.retry:
    xchg eax, [edx]         ; Atomically swap EAX with the value at [edx]
    test eax, eax           ; Was the old value 0?
    jnz .pause_and_retry    ; If it was 1, someone else has the lock
    ret                     ; If it was 0, we now own the lock (and it's now 1)

.pause_and_retry:
    pause                   ; Tell the Ryzen CPU we are in a spin-loop
    jmp .retry

global spin_unlock
spin_unlock:
    mov edx, [esp + 4]
    mov dword [edx], 0      ; Atomic write of 0
    ret

; Macro to create ISR stub without error code
%macro ISR_STUB_NO_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push dword 0        ; fake error code
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_STUB_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push dword %1       ; interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

%macro ISR_PIC_STUB 1
global isr_pic_stub_%1
isr_pic_stub_%1:
    push dword 0        ; fake error code
    push dword %1       ; interrupt number
    jmp isr_pic_stub
%endmacro

%macro ISR_APIC_STUB 1
global isr_apic_stub_%1
isr_apic_stub_%1:
    push dword 0        ; fake error code
    push dword %1       ; interrupt number
    jmp isr_apic_stub
%endmacro

%macro ISR_SYSCALL_STUB 1
global isr_stub_%1
isr_stub_%1:
    push dword 0        ; fake error code
    push dword %1       ; interrupt number
    jmp isr_syscall_stub
%endmacro

global isr_spurious
isr_spurious:
    iret
    

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
ISR_SYSCALL_STUB 128


global isr_common_stub
isr_common_stub:
    pusha               ; save all registers
    push ds
    push es
    push fs
    push gs
    
    mov ax, 0x10       ; load kernel data selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp       
    call isr_handler   
    add esp, 4        

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8         ; clean up int num and error code
    iret

global isr_pic_stub
isr_pic_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4

    mov al, 0x20
    out 0xA0, al      ; slave PIC
    out 0x20, al      ; master PIC

    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8

    iret

global isr_apic_stub
isr_apic_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8

    iret

global isr_syscall_stub
isr_syscall_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call syscall_handler
    add esp, 4
    pop gs
    pop fs
    pop es
    pop ds
    popa

    add esp, 8         ; clean up int num and error code
    iret