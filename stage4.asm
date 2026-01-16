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

    mov eax, 


print_dd_hexa:
    push eax
    push ecx
    push edx
    mov edx, eax        ; Keep original value in EDX
    mov ecx, 8          ; 8 nibbles in a 32-bit doubleword

.loop1:
    rol edx, 4          ; Rotate left 4 bits (brings the highest nibble to the bottom)
    mov eax, edx        ; Copy to EAX
    and al, 0x0F        ; Isolate the lowest 4 bits (the nibble)
    call print_byte_hexa
    loop .loop1          ; Decrement ECX and jump if not zero

    pop edx
    pop ecx
    pop eax
    ret

print_byte_hexa:
    push eax
    push edx
    cmp al, 10
    jl .digit
    add al, 'A' - 10
    jmp .print

.digit
    add al, '0'
.print
    mov edx, [curr_place]
    mov byte [edx], al
    inc edx
    mov byte [edx], 0x07 ; Attribute byte (light grey on black)
    inc edx
    mov [curr_place], edx

    pop edx
    pop eax
    ret


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