[BITS 32]

section .text

global write
write:
    mov eax, 2
    mov ebx, [esp + 4]
    mov ecx, [esp + 8]
    int 0x80
    ret
global read
read:
    mov eax, 3
    mov ebx, [esp + 4]
    mov ecx, [esp + 8]
    mov edx, [esp + 12]
    int 0x80
    ret

global flush_consle_buffer
flush_consle_buffer:
    mov eax, 4
    int 0x80
    ret

global mmap
mmap:
    mov eax, 5
    mov ebx, [esp + 4]
    int 0x80
    ret

global munmap
munmap:
    mov eax, 6
    mov ebx, [esp + 4]
    int 0x80
    ret

global exit
exit:
    mov eax, 1
    int 0x80
    ret