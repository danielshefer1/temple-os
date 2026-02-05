extern isr_handler
extern syscall_handler
extern irq_handler
extern lapic
extern __total_pages
extern kmain

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


global inb
inb:


global outb
outb:


global check_interrupts
check_interrupts:


global load_tss
load_tss:


global switch_to_user_mode
switch_to_user_mode:


global get_cpuid
get_cpuid:


global enable_sse
enable_sse:


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
    mov rdx, rdi
    mov dword [rdi], 0      ; Atomic write of 0
    ret

; Macro to create ISR stub without error code
%macro ISR_STUB_NO_ERROR 1
global isr_stub_%1
isr_stub_%1:

%endmacro

%macro ISR_STUB_ERROR 1
global isr_stub_%1
isr_stub_%1:

%endmacro

%macro ISR_PIC_STUB 1
global isr_pic_stub_%1
isr_pic_stub_%1:

%endmacro

%macro ISR_APIC_STUB 1
global isr_apic_stub_%1
isr_apic_stub_%1:

%endmacro

%macro ISR_SYSCALL_STUB 1
global isr_stub_%1
isr_stub_%1:

%endmacro

global isr_spurious
isr_spurious:
    

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


global isr_pic_stub
isr_pic_stub:


global isr_apic_stub
isr_apic_stub:


global isr_syscall_stub
isr_syscall_stub:
