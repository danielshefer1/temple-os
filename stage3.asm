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

    call clear_screen

    jmp switch_to_real_mode

clear_screen:
    push eax
    push ebx
    push ecx

    mov ebx, VGA_TEXT
    xor eax, eax
    mov ecx, 80 * 25 * 2

.loop_start:
    mov [ebx], eax
    inc ebx
    mov [ebx], 0x07
    inc ebx
    loop .loop_start

    pop ecx
    pop ebx
    pop eax
    ret

stage3_return1:
    mov eax, 0x10      
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax
    call fetch_stage4_data
    jmp switch_to_real_mode

fetch_stage4_data:
    push eax

    ; Debug: Try to print a known value first
    
    mov eax, [STAGE4_BASE]
    mov [sectors_left], eax

    mov eax, [STAGE4_BASE + 4]
    mov [bss_start], eax

    mov eax, [STAGE4_BASE + 8]
    mov [bss_end], eax

    pop eax
    ret

stage3_return2:
    mov eax, 0x10      
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax

    ; 1. Calculate bytes loaded: ECX = sectors_loaded * 512
    mov eax, [sectors_loaded]
    ;call print_dd_hexa
    imul ecx, eax, 512      ; Use imul for 3-operand math
    test ecx, ecx           ; If we somehow loaded 0 bytes, skip
    jz .check_finished

    mov ebx, [user_sectors_left]
    cmp ebx, USER_SECTORS
    jz .load_kernel

    mov edi, [current_user_loading_address]
    jmp .load_con
.load_kernel
    mov edi, [current_kernel_loading_address]
.load_con:
    ; 2. Prepare pointers for copying

    mov esi, 0xA000         ; Source: Temporary Buffer
    
    ; 3. Copy data correctly
    push ecx                ; Save total byte count
    mov ebx, ecx            ; Save copy in EBX for later
    shr ecx, 2              ; Divide by 4 for dword operations
    cld
    rep movsd               ; Copy dwords
    
    mov ecx, ebx            ; Restore original count
    and ecx, 3              ; Get remainder (0-3 bytes)
    rep movsb               ; Copy remaining bytes

    ; 4. Update the global loading pointer for the next track
    pop ecx                 ; Restore total byte count
    mov ebx, [user_sectors_left]
    cmp ebx, USER_SECTORS
    jz .append_kernel
.append_user:
    add [current_user_loading_address], ecx
    jmp .append_con
.append_kernel:
    add [current_kernel_loading_address], ecx

.append_con:

    

    ; 5. Clean up the temporary buffer
    ;mov edi, 0xA000
    ;mov ecx, 16 * 512 / 4   ; Clear max possible buffer size (using /4 for speed)
    ;xor eax, eax
    ;rep stosd               ; Zero out the temporary buffer

.check_finished:
    mov eax, [user_sectors_left]
    test eax, eax
    jz .done                ; If no sectors left, we are finished!

    jmp switch_to_real_mode

.done:

    ; 5. Clear the Kernel's BSS area
    mov edi, [bss_start]
    mov ecx, [bss_end]
    sub ecx, edi            ; ECX = Size of BSS
    js .launch              ; If size is negative, skip to launch
    test ecx, ecx
    jz .launch              ; If size is zero, skip to launch

    xor eax, eax
    cld
    rep stosb               ; Zero the BSS memory at 1MB+

.launch:
    cli
    mov esp, 0x90000
    ; 6. Jump to the Kernel Entry Point at 1MB
    jmp KERNEL_LOADING_ADDRESS


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

go_to_next_line:
    push edx
    push eax
    push ebx

    mov eax, [curr_place]
    sub eax, VGA_TEXT
    xor edx, edx
    mov ebx, 160
    div ebx
    add [curr_place], 160;
    sub [curr_place], edx;

    pop ebx
    pop eax
    pop edx
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
    ; We just came back from fetching metadata or copying a chunk
    mov eax, [user_sectors_left]
    test eax, eax
    jz .all_done                ; If 0, we are finished loading

    call load_section           ; READ the next chunk (Starting at Sector 9)
    jmp switch_to_protected_mode2 ; Go COPY the chunk we just read

.all_done:
    ; This part only hits when sectors_left is 0
    jmp switch_to_protected_mode2 ; Final jump to trigger the BSS/Jump logic
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
    xor eax, eax
    mov es, ax

    mov [sectors_loaded], eax

    ; 2. 32-bit Division for Sector Calculation
    ; Formula: LBA / MAX_SECTOR
    ; Quotient (EAX) = Total Tracks
    ; Remainder (EDX) = Sector Index (0-based)
    
    mov eax, [start_sector]     ; Load 32-bit LBA
    xor edx, edx                ; CLEAR EDX (Critical for 32-bit div!)
    movzx ebx, word [MAX_SECTOR]; Load 16-bit limit into 32-bit reg
    div ebx                     ; EDX:EAX / EBX
    test edx, edx
    jnz .isnt_63
    inc edx

.isnt_63:
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
    test ebx, ebx
    jz .user_sub
    cmp ebx, ecx
    jge .con

.kernel_end:
    mov ecx, ebx
    jmp .con

.user_sub:
    mov ebx, [user_sectors_left]
    cmp ebx, ecx
    jge .con
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
    mov bx, 0xA000              ; Destination Address (Buffer)
    
    int 0x13
    jc .error

    ; 7. Update the LBA for the next loop
    ; Add the number of sectors we actually read (AL) to the start_sector variable
    movzx eax, al               ; Zero-extend AL to 32-bit
    add [start_sector], eax     ; Update the global variable
    mov [sectors_loaded], eax
    mov ebx, [sectors_left]
    cmp ebx, 0
    jz .sub_from_user
    sub [sectors_left], eax 
    jmp .end_of_func

.sub_from_user:
    sub [user_sectors_left], eax

.end_of_func:

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
    mov si, error_msg
    call print_string
    mov al, ah
    call print_hex_byte
    cli
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
STAGE4_SECTOR equ 10
START_SECTOR equ 11
STAGE4_BASE equ 0x8E00
VGA_TEXT equ 0xB8000
USER_SECTORS equ 8

; ----- Data Section -----

sectors_left dd 0
user_sectors_left dd 8
start_sector dd START_SECTOR
boot_drive db 0
sectors_loaded dd 0
user_sectors_loaded dd 0

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

times 2048 -($-$$) db 0