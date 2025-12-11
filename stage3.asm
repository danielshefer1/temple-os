[BITS 32]
global stage3_entry
extern _kernel_LMA_start
extern _kernel_VMA_start
extern _kernel_VMA_end
extern __kernel_sectors
extern kmain      

section .text

    kernel_sectors_count:
        dd __kernel_sectors

stage3_entry:
    ; 1. Reset Segments (Just to be safe)
    mov ax, 0x10        ; Data selector from your GDT
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000    ; Set stack safely below 1MB

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

times 512 - ($ - $$) db 0   ; Pad to ~4 sectors