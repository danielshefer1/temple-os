[BITS 32]
global stage3_entry
extern _kernel_LMA_start
extern _kernel_physical_start
extern _binary_size_text_data ; New symbol
extern _bss_start             ; New symbol
extern _bss_end               ; New symbol
extern __total_sectors
extern __kernel_size_bytes
extern bootstrap_kmain
extern kmain

section .text

stage3_entry:
    mov eax, 0x10      
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax
    mov esp, 0x7C00

    jmp switch_to_real_mode


global enable_paging
enable_paging:
    mov eax, [esp+4]      ; Get page_directory parameter
    mov cr3, eax          ; Load into CR3
    
    mov eax, cr0
    or eax, 0x80000000    ; Set PG bit
    mov cr0, eax
    
    mov eax, [esp+4]
    mov esp, 0x80000000
    add esp, eax
    add esp, 0x2000
    add esp, 0x4FFF

    mov eax, kmain
    jmp eax

    cli
    hlt
    

switch_to_real_mode:
    ; 1. Load 16-bit Data Segment (0x20) into DS, ES, SS
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; 2. Jump to 16-bit Code Segment (0x18)
    ; This "far jump" sets the CS register to 16-bit mode
    jmp 0x18:.pm_16

[BITS 16]
.pm_16:
    ; 3. Disable Protected Mode (Clear PE bit in CR0)
    mov eax, cr0
    and eax, ~1
    mov cr0, eax

    ; 4. Far Jump back to Real Mode Code
    jmp 0x00:.real_entry

.real_entry:
    ; 5. Restore Real Mode Segment Registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    
    ; 6. Re-load the Real Mode IDT (standard 1KB at 0x0)
    lidt [real_mode_idt_desc]
    
    ; 7. Re-enable interrupts
    sti

    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    mov al, '?'
    int 0x10


    
switch_to_protected_mode:
    cli

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:stage3_entry


load_section:
    mov ax, [start_sector]
    add ax, 16
    mov bx, 64

; ----- Define Section -----

USER_LOADING_ADDRESS equ 0x40100000
KERNEL_LOADING_ADDRESS equ 0x100000
START_SECTOR equ 8

; ----- Data Section -----

sectors_left dd __total_sectors
current_kernel_loading_address dd KERNEL_LOADING_ADDRESS
current_user_loading_address dd USER_LOADING_ADDRESS
sectors_loaded dd 0
start_sector dd START_SECTOR

; IDT descriptor for Real Mode
real_mode_idt_desc:
    dw 0x3FF    ; Limit: 1024 bytes
    dd 0        ; Base: 0


times 1024 -($-$$) db 0