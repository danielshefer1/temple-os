[BITS 32]

extern isr_handler

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

global LoadGDTHelper
LoadGDTHelper:
    push eax
    mov eax, [esp + 4] ; pointer to GDTR
    lgdt [eax]
    pop eax
    ret

; Macro to create ISR stub without error code
%macro ISR_STUB_NO_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push dword 0        ; fake error code
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISRs with error code (8, 10-14, 17)
%macro ISR_STUB_ERROR 1
global isr_stub_%1
isr_stub_%1:
    push dword %1       ; interrupt number
    jmp isr_common_stub
%endmacro

; Create stubs for all 32 CPU exceptions
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
ISR_STUB_NO_ERROR 15
ISR_STUB_NO_ERROR 16
ISR_STUB_ERROR 17
ISR_STUB_NO_ERROR 18
ISR_STUB_NO_ERROR 19
ISR_STUB_NO_ERROR 20
ISR_STUB_NO_ERROR 21
ISR_STUB_NO_ERROR 22
ISR_STUB_NO_ERROR 23
ISR_STUB_NO_ERROR 24
ISR_STUB_NO_ERROR 25
ISR_STUB_NO_ERROR 26
ISR_STUB_NO_ERROR 27
ISR_STUB_NO_ERROR 28
ISR_STUB_NO_ERROR 29
ISR_STUB_NO_ERROR 30
ISR_STUB_NO_ERROR 31

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
    
    call isr_handler   ; C handler
    
    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8         ; clean up int num and error code
    iret