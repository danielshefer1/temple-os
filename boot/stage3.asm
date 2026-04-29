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

    mov eax, [STAGE4_BASE] ; bootstrap sectors
    mov [bootstrap_sectors_left], eax    
    call print_dd_hexa


    mov eax, [STAGE4_BASE + 4] ; kernel sectors
    mov [sectors_left], eax
    mov [kernel_sectors], eax
    call print_dd_hexa

    mov eax, [STAGE4_BASE + 8] ; bss start
    mov [bss_start], eax
    call print_dd_hexa

    mov eax, [STAGE4_BASE + 12] ; bss end
    mov [bss_end], eax
    call print_dd_hexa


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
    mov eax, [bootstrap_sectors_loaded]
    ;call print_dd_hexa
    imul ecx, eax, 512      ; Use imul for 3-operand math
    test ecx, ecx           ; If we somehow loaded 0 bytes, skip
    jz .check_finished

    mov ebx, [sectors_left]
    cmp ebx, [kernel_sectors]
    jnz .load_kernel

    mov edi, [current_bootstrap_loading_address]
    jmp .load_con
.load_kernel:
    mov edi, [current_kernel_loading_address]
.load_con:
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
    mov ebx, [sectors_left]
    cmp ebx, [kernel_sectors]
    jnz .append_kernel
.append_bootstrap:
    add [current_bootstrap_loading_address], ecx
    jmp .append_con
.append_kernel:
    add [current_kernel_loading_address], ecx

.append_con:

    

    ; 5. Clean up the temporary buffer
    mov edi, 0xA000
    mov ecx, 16 * 512 / 4   ; Clear max possible buffer size (using /4 for speed)
    xor eax, eax
    rep stosd               ; Zero out the temporary buffer

.check_finished:
    mov eax, [sectors_left]
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
    mov eax, 0xDEADBEEF
    call print_dd_hexa
    jmp (STAGE4_BASE + 16)


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
    call load_stage4
    jmp switch_to_protected_mode1
.loading:
    ; We just came back from fetching metadata or copying a chunk
    mov eax, [sectors_left]
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

    mov word [dap + 2], 1
    mov word [dap + 4], STAGE4_BASE
    mov word [dap + 6], 0
    mov dword [dap + 8], STAGE4_SECTOR
    mov dword [dap + 12], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
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

    xor eax, eax
    mov es, ax

    ; Determine sector count: min(16, remaining)
    mov ecx, 16
    mov ebx, [bootstrap_sectors_left]
    test ebx, ebx
    jz .kernel_sub
    cmp ebx, ecx
    jge .con
    mov ecx, ebx
    jmp .con

.kernel_sub:
    mov ebx, [sectors_left]
    cmp ebx, ecx
    jge .con
    mov ecx, ebx

.con:
    ; Build DAP for LBA read
    mov [dap + 2], cx
    mov word [dap + 4], 0xA000
    mov word [dap + 6], 0
    mov eax, [start_sector]
    mov [dap + 8], eax
    mov dword [dap + 12], 0

    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .error

    movzx eax, cx               ; Sectors actually requested
    add [start_sector], eax
    mov [bootstrap_sectors_loaded], eax
    mov ebx, [bootstrap_sectors_left]
    test ebx, ebx
    jz .sub_from_kernel
    sub [bootstrap_sectors_left], eax
    jmp .end_of_func

.sub_from_kernel:
    sub [sectors_left], eax

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

; Disk Address Packet for INT 0x13 / AH=0x42
align 4
dap:
    db 0x10        ; size of packet
    db 0           ; reserved
    dw 0           ; sector count
    dw 0           ; transfer offset
    dw 0           ; transfer segment
    dq 0           ; starting LBA

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

BOOTSTRAP_LOADING_ADDRESS equ 0x100000
KERNEL_LOADING_ADDRESS equ 0x200000
BOOT_DRIVE equ 0x85F0
STAGE4_SECTOR equ 9
START_SECTOR equ 10
STAGE4_BASE equ 0x8E00
VGA_TEXT equ 0xB8000

; ----- Data Section -----

sectors_left dd 0
kernel_sectors dd 0
bootstrap_sectors_left dd 0
start_sector dd START_SECTOR
boot_drive db 0
sectors_loaded dd 0
bootstrap_sectors_loaded dd 0

current_kernel_loading_address dd KERNEL_LOADING_ADDRESS
current_bootstrap_loading_address dd BOOTSTRAP_LOADING_ADDRESS

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