[BITS 32]

section .text

global write
write:
    mov eax, 2
    mov ebx, [esp + 4]
    int 0x80
    ret

global hlt_syscall
hlt_syscall:
    mov eax, 1
    int 0x80
    ret