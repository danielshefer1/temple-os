%include "./build/offsets.inc"
[BITS 32]
enable_long_mode_and_jump:
    mov eax, [esp + 4]        ; Get PML4 physical address from parameter
    mov cr3, eax              ; Load PML4 into CR3

    ; 1. Enable PAE (Physical Address Extension) - Mandatory for 64-bit
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

    ; --- At this point, you are in "Compatibility Mode" (32-bit code, 64-bit paging) ---

    ; 4. Far Jump to enter true 64-bit Long Mode
    ; We must use a 64-bit Global Descriptor Table (GDT) entry
    push 0x28                 
    push long_mode_entry   
    retf

[BITS 64]
long_mode_entry:
    mov rax, 0x30
    mov ds, rax
    mov es, rax
    mov fs, rax
    mov gs, rax
    mov ss, rax

    mov rsp, 0xFFFFFFFF80400000 

    ; 6. Jump to your Kernel
    mov rax, KERNEL_ENTRY_64
    jmp rax

    ; Safety net
    cli
    hlt