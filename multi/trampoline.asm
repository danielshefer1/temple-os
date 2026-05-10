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

[BITS 32]
ap_kernel_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    mov esp, 0x9FC00 ; Set up a stack (arbitrary choice, must be below 1MB)


    mov eax, [0xA210]        ; Get PML4 physical address from parameter
    mov cr3, eax              

    ; 1. Enable PAE 
    mov eax, cr4
    or eax, 1 << 5            ; Set bit 5
    mov cr4, eax

    ; 2. Enable Long Mode in the EFER MSR
    mov ecx, 0xC0000080       ; EFER MSR constant
    rdmsr                     ; Read Model Specific Register into EDX:EAX
    or eax, 1 << 8            ; Set LME (Long Mode Enable) bit
    or eax, 1 << 11   ; Set NXE bit
    wrmsr                     ; Write it back
    
    ; 3. Enable Paging (The actual transition)
    mov eax, cr0
    or eax, 1 << 31           ; Set PG (Paging) bit
    mov cr0, eax

    add eax, 0
    ;cli
    ;hlt
    ;jmp $

    push 0x28                 
    push long_mode_entry
    retf

[BITS 64]
default abs

long_mode_entry:
    mov rax, 0x30
    mov ds, rax
    mov es, rax
    mov fs, rax
    mov gs, rax
    mov ss, rax



    mov rsp, [0xA200]

    mov rax, [0xA208]
    jmp rax

    ; Safety net
    cli
    hlt

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
    db 0x00, 10011010b, 00001111b, 0x00 ; 

    ; 4. 16-bit Data (0x20) - For returning to Real Mode
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 00001111b, 0x00 

    ; 0x28: 64-bit Code (The "Golden" Selector)
    ; Base: 0, Limit: 0 (ignored), Access: 0x9A, Flags: 0x2 (L-bit set)
    dw 0x0000, 0x0000
    db 0x00, 10011010b, 00100000b, 0x00 

    ; 0x30: 64-bit Data
    ; Base: 0, Limit: 0, Access: 0x92, Flags: 0x0
    dw 0x0000, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size (Limit) of GDT (always 1 less than true size)
    dd (gdt_start + 0)               ; Base address of GDT

times 512 - ($ - $$) db 0