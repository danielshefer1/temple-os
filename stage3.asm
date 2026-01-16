[ORG 0x8600]
[BITS 32]

stage3_entry:
    mov eax, 0x10      
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax
    mov esp, 0x7C00

    mov bx, BOOT_DRIVE
    mov al, [BOOT_DRIVE]
    mov [boot_drive], al

    jmp switch_to_real_mode

stage3_return1:
    call fetch_stage4_data
    jmp switch_to_real_mode

fetch_stage4_data:
    push eax

    mov eax, [STAGE4_BASE]
    mov [sectors_left], eax

    mov eax, [STAGE4_BASE + 4]
    mov [bss_start], eax

    mov eax, [STAGE4_BASE + 8]
    mov [bss_end], eax

    pop eax
    ret

stage3_return2:
    ; Copying to High Adddress 
    mov eax, [sectors_left]
    test eax, eax
    jz done
    mov eax, [sectors_loaded]
    imul ecx, eax, 512
    push ecx
    mov edi, [current_kernel_loading_address]
    mov [current_kernel_loading_address], edi
    mov esi, 0x9000
    cld
    rep movsb

    pop ecx
    add [current_kernel_loading_address], ecx
    ; Cleaning Up the lower addresses
    mov edi, 0x9000
    xor eax, eax
    rep stosb

    jmp switch_to_real_mode

done:
    mov edi, bss_start
    mov ecx, bss_end
    sub ecx, bss_start ; Calculate BSS size
    xor eax, eax        ; Value to write (0)
    rep stosb           ; Store AL (0) into [EDI] ECX times

    mov eax, STAGE4_BASE + 12
    jmp eax


print_string_protected:
    lodsb
    test al, al
    jz .done_print_protected
    mov byte [edx], al
    inc edx
    mov byte [edx], 0x07 ; Attribute byte (light grey on black)
    inc edx
    jmp print_string_protected
.done_print_protected:
    ret


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
    jmp 0x18:pm_16

[BITS 16]
pm_16:
    ; 3. Disable Protected Mode (Clear PE bit in CR0)
    mov eax, cr0
    and eax, ~1
    mov cr0, eax

    ; 4. Far Jump back to Real Mode Code
    jmp 0x0000:real_entry

real_entry:
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

    xor ax, ax
    mov bx, start
    mov al, byte [bx]
    mov ah, al
    test al, al
    jnz .loading
.setup:
    mov al, 1
    mov byte [bx], al
    call get_drive_geometry
    call load_stage4
    jmp switch_to_protected_mode1
.loading:
    mov eax, [sectors_left]
    test eax, eax
    jz .skip
    call load_section

.skip:
    jmp switch_to_protected_mode2

    cli
    hlt

switch_to_protected_mode1:
    cli

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:stage3_return1
    
switch_to_protected_mode2:
    cli

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:stage3_return2

load_stage4:
    push es
    pusha

    xor ax, ax
    mov es, ax
    mov bx, STAGE4_BASE
    mov cl, STAGE4_SECTOR
    mov al, 1
    mov dh, 0
    mov dl, [boot_drive]
    mov ch, 0
    mov ah, 0x02
    int 0x13
    jc .error

    popa
    pop es
    ret

.error:
    mov si, error_msg
    call print_string
    mov al, ah
    call print_hex_byte
    cli
    hlt

load_section:
    push es
    pusha

    ; 1. Setup Buffer Segment
    xor ax, ax
    mov es, ax

    ; 2. 32-bit Division for Sector Calculation
    ; Formula: LBA / MAX_SECTOR
    ; Quotient (EAX) = Total Tracks
    ; Remainder (EDX) = Sector Index (0-based)
    
    mov eax, [start_sector]     ; Load 32-bit LBA
    xor edx, edx                ; CLEAR EDX (Critical for 32-bit div!)
    movzx ebx, word [MAX_SECTOR]; Load 16-bit limit into 32-bit reg
    div ebx                     ; EDX:EAX / EBX
    
    inc edx                     ; Convert 0-based remainder to 1-based Sector
    mov [current_sector_val], dx ; Save standard sector number for later

    ; 3. Calculate "Safe Read Count"
    ; We can only read up to the end of the current track.
    ; SafeCount = MAX_SECTOR - CurrentSector + 1
    mov cx, [MAX_SECTOR]
    sub cx, dx                  ; Remaining sectors after this one
    inc cx                      ; Include this one
    ; If remaining sectors > 16, just read 16. Otherwise read whatever is left.
    cmp cx, 16
    jle .limit_calculated
    mov cx, 16                  ; Cap read size at 16

.limit_calculated:
    movzx ecx, cx
    mov ebx, [sectors_left]
    cmp ecx, ebx
    jge .done
    jmp .con 

.done:
    mov ecx, ebx
.con:
    push cx                     ; Save [Count] for INT 0x13
    
    ; 4. Calculate Cylinder and Head
    ; EAX currently holds "Total Tracks".
    ; Formula: TotalTracks / MAX_HEAD
    ; Quotient (EAX) = Cylinder
    ; Remainder (EDX) = Head
    
    xor edx, edx                ; Clear EDX again!
    movzx ebx, word [MAX_HEAD]
    div ebx
    
    ; Now: EAX = Cylinder, EDX = Head
    
    ; 5. Construct BIOS Registers
    ; CH = Cylinder Low, DH = Head, CL = Sector
    mov dh, dl                  ; Head
    mov ch, al                  ; Cylinder Low 8 bits

    ; Handle Cylinder High Bits (Bits 8-9 go to CL bits 6-7)
    mov dl, ah                  ; Get Cylinder High bits
    shl dl, 6                   ; Shift to top position
    
    mov bx, [current_sector_val]; Retrieve the sector number we calculated
    or dl, bl                   ; Combine High Cyl bits with Sector
    mov cl, dl                  ; Move to CL
    
    ; 6. Perform the Read
    pop ax                      ; Restore [Count] into AL
    mov ah, 0x02                ; Read Sectors Function
    mov dl, [boot_drive]        ; Drive ID
    mov bx, 0x9000              ; Destination Address (Buffer)
    
    int 0x13
    jc .error

    ; 7. Update the LBA for the next loop
    ; Add the number of sectors we actually read (AL) to the start_sector variable
    movzx eax, al               ; Zero-extend AL to 32-bit
    add [start_sector], eax     ; Update the global variable
    mov [sectors_loaded], eax
    sub [sectors_left], eax

    popa
    pop es
    ret

.error:
    mov si, error_msg
    call print_string
    mov al, ah
    call print_hex_byte
    cli
    hlt

; Temporary storage for the calculated sector (since we need registers for division)
current_sector_val dw 0

; Function: get_drive_geometry
; Returns:
;   BL = Drive Type
;   CH = Max Cylinder (low 8 bits)
;   CL = Max Sector (bits 0-5), Max Cylinder (high 2 bits in 6-7)
;   DH = Max Head
get_drive_geometry:
    push es             ; BIOS might change ES
    xor di, di          ; Set DI to 0 to avoid BIOS bugs
    mov es, di
    mov ah, 0x08        ; Get drive parameters
    mov dl, [boot_drive]; Usually 0x80
    int 0x13
    jc .error           ; Carry flag set on error
    
    ; Extracting values:
    ; Sectors per track are in CL bits 0-5
    push cx
    and cx, 0x003F      ; Keep only bits 0-5 for max sector
    mov [MAX_SECTOR], cx
    pop cx
    
    movzx dx, dh
    inc dx              ; DH is 'Max Head Index', so Heads = DH + 1
    mov [MAX_HEAD], dx

    and cl, 0xC0
    xor ax, ax
    mov al, cl
    shl ax, 2
    mov al, ch
    mov [MAX_CYLINDER], ax
    
    pop es
    ret

.error:
    ; Handle error (e.g., print a message)
    hlt

print_tab:
    mov cx, 4
    mov ah, ' '
.start:
    push cx
    push ax
    mov al, ah
    mov ah, 0x0E
    mov bh, 0   
    mov bl, 0x07        
    int 0x10
    pop ax
    pop cx
    loop .start

    ret

print_string:
    lodsb
    test al, al
    jz .done_print_real
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp print_string
.done_print_real:
    ret

print_hex_word:
    push ax
    push bx

    mov bx, ax
    shr ax, 8
    call print_hex_byte
    mov ax, bx
    call print_hex_byte

    pop bx
    pop ax
    ret 

print_hex_byte:
    push ax
    push bx
    
    ; Print high nibble
    mov ah, al
    shr ah, 4           ; Get upper 4 bits
    call print_hex_digit
    
    ; Print low nibble
    mov ah, al
    and ah, 0x0F        ; Get lower 4 bits
    call print_hex_digit
    
    pop bx
    pop ax
    ret

print_hex_digit:
    ; AH contains a value 0-15
    cmp ah, 9
    jg .letter
    add ah, '0'         ; Convert 0-9 to ASCII
    jmp .print
.letter:
    add ah, 'A' - 10    ; Convert 10-15 to A-F
.print:
    push ax
    mov al, ah
    mov ah, 0x0E
    mov bh, 0   
    mov bl, 0x07        
    int 0x10
    pop ax
    ret

; ----- Define Section -----

USER_LOADING_ADDRESS equ 0x40100000
KERNEL_LOADING_ADDRESS equ 0x100000
BOOT_DRIVE equ 0x85F0
STAGE4_SECTOR equ 8
START_SECTOR equ 9
STAGE4_BASE equ 0x8A00
VGA_TEXT equ 0xB8000

; ----- Data Section -----

sectors_left dd 0
start_sector dd START_SECTOR
boot_drive db 0
sectors_loaded dw 0

current_kernel_loading_address dd KERNEL_LOADING_ADDRESS
current_user_loading_address dd USER_LOADING_ADDRESS

MAX_SECTOR dw 0
MAX_HEAD dw 0
MAX_CYLINDER dw 0

bss_start dd 0
bss_end dd 0

start db 0


curr_place dd VGA_TEXT

; IDT descriptor for Real Mode
real_mode_idt_desc:
    dw 0x3FF    ; Limit: 1024 bytes
    dd 0        ; Base: 0

error_msg:
    db "Loading Error! Error Code: ", 0

result_msg:
    db "Loaded Kernel Sectors! ", 0

times 1024 -($-$$) db 0