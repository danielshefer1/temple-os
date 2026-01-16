extern __total_sectors
extern _bss_start
extern _bss_end
extern kmain
extern bootstrap_kmain

section .text

global stage4_entry
stage4_entry:
    dd __total_sectors
    dd _bss_start
    dd _bss_end

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

times 512 - ($ - $$) db 0