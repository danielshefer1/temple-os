[bits 32]
section .trampoline
global trampoline_binary
global trampoline_size

trampoline_binary:
    incbin "./build/trampoline.bin"
trampoline_binary_end:

trampoline_size:
    dd trampoline_binary_end - trampoline_binary