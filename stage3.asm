[BITS 32]
global stage3_entry
extern _kernel_LMA_start
extern _kernel_VMA_start
extern _kernel_VMA_end
extern __total_sectors
extern kmain      

section .text

stage3_entry:
    ; 1. Reset Segments (Just to be safe)
    mov ax, 0x10        ; Data selector from your GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000    ; Set stack safely below 1MB
    mov ax, 0x00

    mov edx, 0xB8000   ; VGA text mode memory (video memory base)
    ; Calculate middle of screen: (row * 80 + col) * 2
    ; Row 14, Column 30 = (14 * 80 + 30)
    add edx, 2360    ; Move to middle of screen
    mov [edx], 'L'
    inc edx
    mov byte [edx], 0x07 ; Attribute byte (light grey on black
    inc edx

    ; 2. Copy C Kernel to 1MB
    ; We must manually move the code because the BIOS loaded it 
    ; linearly after 0x8600, but the C code was linked to run at 0x100000.
    
    mov esi, _kernel_LMA_start  ; Source: Where BIOS loaded it (Linear address)
    mov edi, _kernel_VMA_start  ; Dest:   1MB (0x100000)
    
    mov ecx, _kernel_VMA_end    ; Calculate size...
    sub ecx, _kernel_VMA_start
    
    cld                         ; Clear direction flag (forward copy)
    rep movsb                   ; Copy ECX bytes from ESI to EDI

    ; 3. Jump to C Kernel
    call kmain

    ; 4. Hang if kernel returns
    cli
    hlt