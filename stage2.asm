%ifidn __OUTPUT_FORMAT__, bin
    [ORG 0x7E00]
%endif

[BITS 16]

; ----- Stage 2 Bootloader -----

start:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_ADDR         ; Set up stack
    ; Store boot drive number
    mov [BOOT_DRIVE], dl

    ;mov si, message
    ;call print_string

    ; ===== REAL MODE OPERATIONS =====
    ; These must be done before switching to protected mode
    
    ; Enable A20 line (allows access to memory up to 1MB)
    call enable_a20
    
    ; Get E820 memory map from BIOS
    call get_e820_memory_map
    
    ; Remap PIC (Programmable Interrupt Controller)
    call remap_pic
    
    mov bx, STAGE3_ADDR      ; Destination address (ES:BX)
    mov cl, 6           ; Start at Sector 6
    mov al, 2        
    call load_file


no_additional_sectors:
    ; ===== ENTER PROTECTED MODE =====

    ; Disable interrupts
    cli
    
    ; Load the GDT
    lgdt [gdt_descriptor]
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to flush the prefetch queue and enter protected mode
    jmp 0x08:STAGE3_ENTRY

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

; ----- A20 Line Enable -----
enable_a20:
    ; Use BIOS interrupt to enable A20
    mov ax, 0x2401
    int 0x15
    ret

; ----- E820 Memory Map -----
get_e820_memory_map:
    ; Get memory map from BIOS and store at 0x500
    ; Memory info structure:
    ; 0x500: signature ('SMAP')
    ; 0x504: entry count (dword)
    ; 0x508: address of E820 entries (dword)
    
    mov dword [MEMORY_MAP_ADDR], 0x534D4150   ; 'SMAP' signature
    mov dword [MEMORY_MAP_COUNT], 0             ; Entry counter starts at 0
    
    xor ebx, ebx                     ; EBX = 0 (continuation value)
    mov di, MEMORY_MAP_ENTRIES                    ; Store entries starting at 0x520
    xor cx, cx                       ; Entry counter
    
.e820_loop:
    push cx
    mov eax, 0xE820                  ; E820 function
    mov ecx, 20                      ; Entry size
    mov edx, 0x534D4150              ; 'SMAP' signature
    int 0x15                         ; Call BIOS
    jc .e820_done                    ; If carry flag set, we're done
    pop cx

    add di, 20                       ; Move to next entry slot
    inc cx                           ; Increment entry counter
    cmp cx, 32                       ; Safety limit: max 32 entries
    je .e820_done                    ; If limit reached, stop
    
    test ebx, ebx                    ; If EBX is 0, list is complete
    jnz .e820_loop
    
.e820_done:
    and ecx, 0x0000FFFF             ; Limit entry count to 4095
    mov [0x504], ecx                 ; Store entry count at 0x504
    mov dword [0x508], 0x520         ; Store entry address at 0x508
    ret

; ----- PIC Remap -----
remap_pic:
    ; Remap PIC so IRQs don't conflict with CPU exceptions
    ; IRQ 0-7 -> INT 32-39
    ; IRQ 8-15 -> INT 40-47
    
    ; ICW1 - Start initialization
    mov al, 0x11
    out 0x20, al                     ; Send to master PIC
    out 0xA0, al                     ; Send to slave PIC
    
    ; ICW2 - Set interrupt vectors
    mov al, 32                       ; Master PIC: IRQs start at INT 32
    out 0x21, al
    mov al, 40                       ; Slave PIC: IRQs start at INT 40
    out 0xA1, al
    
    ; ICW3 - Set up cascading
    mov al, 4                        ; Master PIC: slave at IRQ2
    out 0x21, al
    mov al, 2                        ; Slave PIC: connected to IRQ2
    out 0xA1, al
    
    ; ICW4 - Final configuration
    mov al, 1
    out 0x21, al
    out 0xA1, al
    
    mov al, 0xFC                ; Mask all except IRQ0 (timer) and IRQ1 (keyboard)
    out 0x21, al                     ; Master PIC
    mov al, 0xFF                     ; Mask all on slave
    out 0xA1, al                     ; Slave PIC
    
    ret

; INPUT
; bx - destination address (ES:BX)
; cl - sector
; al - number of sectors to read
; OUTPUT
; Reads sectors into ES:BX
load_file:
    push bx
    push es
    push dx
    push ax
    push cx


    mov dh, 0           ; Head 0
    mov dl, [BOOT_DRIVE]; Drive number (saved from DL at boot)
    mov ch, 0           ; Cylinder 0
    mov ah, 0x02        ; BIOS Read Sector function
    int 0x13
    
    jc .disk_error       ; Handle error if carry flag set
    jmp .file_loaded

.disk_error:
    ; Print error message and halt
    mov si, disk_error_msg
    call print_string
    shr ax, 8
    call print_hex_byte
    hlt

.file_loaded:
    pop cx
    pop ax
    pop dx
    pop es
    pop bx
    ret

; ----- Data Section -----

message:
    db "Stage 2 Bootloader!", 0

message_protected:
    db "Hello from Protected Mode!", 0

disk_error_msg:
    db "Disk Read Error!", 0

BOOT_DRIVE db 0
KERNEL_SIZE dd 0

STACK_ADDR equ 0x7C00

MEMORY_MAP_ADDR equ 0x500
MEMORY_MAP_COUNT equ 0x504
MEMORY_MAP_ENTRIES equ 0x520

STAGE3_ADDR equ 0x8600
STAGE3_ENTRY equ 0x8600
; ----- GDT Setup -----

gdt_start:
    ; 0. Null Descriptor
    dq 0

    ; 1. 32-bit Code (0x08)
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00

    ; 2. 32-bit Data (0x10)
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00

    ; 3. 16-bit Code (0x18) - For returning to Real Mode
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 00001111b, 0x00 ; D bit is 0 (16-bit)

    ; 4. 16-bit Data (0x20) - For returning to Real Mode
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 00001111b, 0x00 ; D bit is 0 (16-bit)
gdt_end:

; 4. GDT Descriptor (This is what you load into the CPU using LGDT)
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size (Limit) of GDT (always 1 less than true size)
    dd gdt_start                ; Base address of GDT

; No boot signature needed for Stage 2
times 2048 - ($ - $$) db 0   ; Pad to ~4 sectors