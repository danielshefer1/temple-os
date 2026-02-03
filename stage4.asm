%include "./build/offsets.inc"
extern __bootstrap_sectors
extern bootstrap_kmain

section .stage4

global stage4_entry
global kernel_sectors
stage4_entry:
    dd __bootstrap_sectors
    kernel_sectors dd KERNEL_SECTORS
    dd KERNEL_BSS_START
    dd KERNEL_BSS_END

    mov eax, bootstrap_kmain
    call print_dd_hexa

    jmp eax

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
    loop .loop1
    mov edx, [curr_place]
    add edx, 4
    mov [curr_place], edx

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

.digit:
    add al, '0'
.print:
    mov edx, [curr_place]
    mov byte [edx], al
    inc edx
    mov byte [edx], 0x07 ; Attribute byte (light grey on black)
    inc edx
    mov [curr_place], edx

    pop edx
    pop eax
    ret

curr_place dd 0xB8000

global enable_long_mode_and_jump
enable_long_mode_and_jump:
    incbin "./build/long_mode_init.bin"



times 512 - ($ - $$) db 0