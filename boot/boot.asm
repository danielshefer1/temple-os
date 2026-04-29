[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    sti
    mov [boot_drive], dl    ; Save the drive ID passed by BIOS

    ; Load Stage 2 from disk via LBA (INT 0x13, AH=0x42)
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13

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

boot_drive db 0

dap:
    db 0x10        ; size of packet
    db 0           ; reserved
    dw 8           ; sectors to read (stage 2 + stage 3)
    dw 0x7E00      ; transfer offset
    dw 0           ; transfer segment
    dq 1           ; starting LBA (sector 2 = LBA 1)

times 446-($-$$) db 0

; Partition Entry 1 (16 bytes)
db 0x80                 ; Bootable
db 0, 0, 0              ; Starting CHS (ignored)
db 0x83                 ; Type (0x83 = Linux/Generic Data)
db 0, 0, 0              ; Ending CHS (ignored)
dd 9                    ; STARTING LBA (Matches your 'seek=9' in Makefile!)
dd 40951                ; SIZE IN SECTORS (Total sectors - 9)

; Fill remaining 3 entries with zeros (48 bytes)
times 48 db 0

dw 0xAA55               ; The Magic Signature