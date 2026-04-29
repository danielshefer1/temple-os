[BITS 16]
[ORG 0x0600]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [boot_drive], dl    ; Save the drive ID passed by BIOS

    ; Load Stage 2 + Stage 3 from disk via LBA (INT 0x13, AH=0x42)
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13

    jc error                  ; Jump if carry flag set (error)

    ; Far jump to Stage 2
    mov dl, [boot_drive]
    jmp 0x0000:0x7E00

error:
    mov si, err_msg
    call print_string
    hlt

err_msg:
    db "Load Error!", 0

print_string:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp print_string
.done:
    ret

boot_drive db 0

dap:
    db 0x10        ; size of packet
    db 0           ; reserved
    dw 8           ; sectors to read (stage 2 + stage 3)
    dw 0x7E00      ; transfer offset
    dw 0           ; transfer segment
    dq 2           ; starting LBA (stage2 begins at LBA 2)