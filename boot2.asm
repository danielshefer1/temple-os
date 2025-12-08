[BITS 16]
[ORG 0x7E00]

start:
    mov si, message
    call print_string
    hlt
    jmp $

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp print_string
.done:
    ret

message:
    db "Stage 2 Bootloader!", 0

; No boot signature needed for Stage 2
times 2048 - ($ - $$) db 0   ; Pad to ~4 sectors