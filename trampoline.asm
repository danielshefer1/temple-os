[ORG 0xA000]
[BITS 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    
    lgdt [gdt_descriptor] ; Load GDT
    
    mov eax, cr0
    or eax, 1
    mov cr0, eax ; Enter Protected Mode
    
    ; Jump to 32-bit entry point
    jmp 0x08:ap_kernel_entry

[bits 32]
ap_kernel_entry:
    mov ax, 0x10    
    mov ds, ax
    mov es, ax
    mov ss, ax      
    mov fs, ax
    mov gs, ax

    mov esp, [0xA200] ; The Stack pointer for the cpu
    mov edx, [0xA204] ; A pointer to the start function
    mov eax, [0xA208] ; Page Directory address
    mov cr3, eax

    mov eax, cr0
    or eax, 0x80000000    ; Set PG bit
    mov cr0, eax
    
    add esp, 0xC0000000
    jmp edx      
    jmp $

gdt_start:
    ; 0. Null Descriptor
    dq 0

    ; 1. 32-bit Code (0x08)
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00

    ; 2. 32-bit Data (0x10)
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00

    ; 3. 16-bit Code (0x18) - For returning to Real Mode
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 00001111b, 0x00 ; D bit is 0 (16-bit)

    ; 4. 16-bit Data (0x20) - For returning to Real Mode
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 00001111b, 0x00 ; D bit is 0 (16-bit)
gdt_end:

; 4. GDT Descriptor (This is what you load into the CPU using LGDT)
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size (Limit) of GDT (always 1 less than true size)
    dd gdt_start                ; Base address of GDT

times 512 - ($ - $$) db 0