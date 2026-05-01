[BITS 64]

section .text
global _start
_start:
    mov rax, 99
    syscall

    mov rax, 1
    syscall

.hang:
    jmp .hang
