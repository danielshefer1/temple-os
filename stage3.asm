[BITS 32]
global stage3_entry
extern _kernel_LMA_start
extern _kernel_VMA_start
extern _binary_size_text_data ; New symbol
extern _bss_start             ; New symbol
extern _bss_end               ; New symbol
extern __total_sectors
extern kmain      

section .text

stage3_entry:
    dd __total_sectors  ; Total sectors read so far (from stage 2)
    ; 1. Setup Segments & Stack...
    mov ax, 0x10
    mov ss, ax
    mov esp, 0x90000

    mov ebx, 0x0B8000  ; VGA text mode memory (video memory base)
    ; Calculate middle of screen: (row * 80 + col) * 2
    ; Row 14, Column 30 = (14 * 80 + 30)
    add ebx, 2000      ; Move to middle of screen
    mov byte [ebx], 'L'
    inc ebx
    mov byte [ebx], 0x07 ; Attribute byte (light grey on black
    inc ebx
    ; 2. Copy Code & Data (Exclude BSS)
    mov esi, _kernel_LMA_start 
    mov edi, _kernel_VMA_start
    mov ecx, _binary_size_text_data ; Only copy what exists in the file
    cld
    rep movsb

    ; 3. Zero out the BSS (Initialize variables to 0)
    mov edi, _bss_start
    mov ecx, _bss_end
    sub ecx, _bss_start ; Calculate BSS size
    xor eax, eax        ; Value to write (0)
    rep stosb           ; Store AL (0) into [EDI] ECX times

    ; 4. Jump to Kernel
    mov eax, kmain
    call eax

    cli
    hlt