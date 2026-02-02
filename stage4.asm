%include "offsets.inc"
extern _bootstrap_sectors


section .stage4

global stage4_entry
stage4_entry:
    dd _bootstrap_sectors
    dd KERNEL_SECTORS
    dq KERNEL_ENTRY_64
    dq KERNEL_BSS_START
    dq KERNEL_BSS_END

global enable_paging_bootstrap
enable_paging_bootstrap:
    mov eax, [esp+4]      ; Get page_directory parameter
    mov cr3, eax          ; Load into CR3
    
    mov eax, cr0
    or eax, 0x80000000    ; Set PG bit
    mov cr0, eax
    
    mov eax, [esp+4]
    mov esp, 0xC0000000
    add esp, eax
    add esp, 0x2000
    add esp, 0x4FFF

    mov eax, kmain
    jmp eax

    cli
    hlt

global enable_paging
enable_paging:
    mov eax, [esp+4]      ; Get page_directory parameter
    mov cr3, eax          ; Load into CR3
    
    mov eax, cr0
    or eax, 0x80000000    ; Set PG bit
    mov cr0, eax

    add esp, 0xC0000000
    ret



times 512 - ($ - $$) db 0