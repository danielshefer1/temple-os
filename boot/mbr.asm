[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Probe drive presence: AH=0x08 Get Drive Parameters
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    mov [err_geo], ah

    ; Try LBA extended read
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc done
    mov [err_lba], ah

    ; Try CHS: LBA 1 = CHS (cyl=0, head=0, sector=2)
    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    mov bx, 0x0600
    int 0x13
    jnc done
    mov [err_chs], ah

    jmp error

done:
    mov dl, [boot_drive]
    jmp 0x0000:0x0600

error:
    mov si, err_msg
    call print_string
    mov al, [boot_drive]
    call print_hex_byte
    mov si, geo_msg
    call print_string
    mov al, [err_geo]
    call print_hex_byte
    mov si, lba_msg
    call print_string
    mov al, [err_lba]
    call print_hex_byte
    mov si, chs_msg
    call print_string
    mov al, [err_chs]
    call print_hex_byte
.hang:
    hlt
    jmp .hang

err_msg: db "DL=", 0
geo_msg: db " GEO=", 0
lba_msg: db " LBA=", 0
chs_msg: db " CHS=", 0

print_hex_byte:
    push ax
    push bx
    mov bl, al
    shr al, 4
    call print_hex_digit
    mov al, bl
    and al, 0x0F
    call print_hex_digit
    pop bx
    pop ax
    ret

print_hex_digit:
    cmp al, 9
    jle .d
    add al, 'A'-10
    jmp .o
.d: add al, '0'
.o: mov ah, 0x0E
    mov bh, 0
    int 0x10
    ret

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
err_geo    db 0
err_lba    db 0
err_chs    db 0

dap:
    db 0x10        ; size of packet
    db 0           ; reserved
    dw 1           ; sectors
    dw 0x0600      ; transfer offset
    dw 0           ; transfer segment
    dq 1           ; starting LBA

times 446-($-$$) db 0

; Partition Entry 1
db 0x80                 ; Bootable
db 0, 0, 0              ; Starting CHS (ignored)
db 0x83                 ; Type
db 0, 0, 0              ; Ending CHS (ignored)
dd 1                    ; Starting LBA
dd 40959                ; Size in sectors

times 48 db 0

dw 0xAA55
