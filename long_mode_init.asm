%include "./build/offsets.inc"
[BITS 32]
enable_long_mode_and_jump:
    mov eax, [esp + 4]        ; Get PML4 physical address from parameter
    mov cr3, eax              

    ; 1. Enable PAE 
    mov eax, cr4
    or eax, 1 << 5            ; Set bit 5
    mov cr4, eax

    ; 2. Enable Long Mode in the EFER MSR
    mov ecx, 0xC0000080       ; EFER MSR constant
    rdmsr                     ; Read Model Specific Register into EDX:EAX
    or eax, 1 << 8            ; Set LME (Long Mode Enable) bit
    wrmsr                     ; Write it back
    
    ; 3. Enable Paging (The actual transition)
    mov eax, cr0
    or eax, 1 << 31           ; Set PG (Paging) bit
    mov cr0, eax

    call .get_eip
.get_eip:
    pop eax                      ; EAX now holds the *actual* address of .get_eip
    add eax, (long_mode_entry - .get_eip) ; Add the relative distance to the label

    push 0x28                 
    push eax
    retf

[BITS 64]
long_mode_entry:
    mov rax, 0x30
    mov ds, rax
    mov es, rax
    mov fs, rax
    mov gs, rax
    mov ss, rax

    mov rax, KERNEL_BSS_END
    add rax, 0xFFFFFFFF80000000
    sub rax, 0x200000
    add rax, 0xFFF           
    and rax, 0xFFFFFFFFFFFFF000 
    add rax, 0x4000

    mov rsp, rax

    mov qword [0xB8000], 0x0720072007200720  ; Print spaces

    mov rax, KERNEL_ENTRY_64
    jmp rax

    ; Safety net
    cli
    hlt