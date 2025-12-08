[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    sti

    ; Load Stage 2 from disk
    mov ah, 0x02              ; BIOS read sector function
    mov al, 4                 ; Number of sectors to read (Stage 2 size)
    mov ch, 0                 ; Cylinder 0
    mov cl, 2                 ; Start at sector 2 (sector 1 is boot sector)
    mov dh, 0                 ; Head 0
    mov dl, 0x80              ; Drive 0 (first hard drive)
    mov bx, 0x7E00            ; Load to 0x7E00 (right after Stage 1)
    int 0x13                  ; BIOS disk interrupt

    jc error                  ; Jump if carry flag set (error)

    ; Jump to Stage 2
    jmp 0x7E00

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

times 510 - ($ - $$) db 0
dw 0xAA55