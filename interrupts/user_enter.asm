[BITS 64]

section .text

global enter_user_mode
enter_user_mode:
    mov ax, 0x1B
    mov ds, ax
    mov es, ax
    mov fs, ax

    push qword 0x1B
    push rsi
    push qword 0x202
    push qword 0x23
    push rdi
    iretq
