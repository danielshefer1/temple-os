#!/bin/sh
# One-time build of the user-mode SYSCALL test program and install onto data.img.
# Re-run only when user/test_syscall.S changes.
set -e

OUT=user/out
mkdir -p "$OUT"

nasm -f elf64 user/test_syscall.S -o "$OUT/test_syscall.o"
x86_64-elf-ld -m elf_x86_64 -T user/user_linker.ld -nostdlib \
    -o "$OUT/user.elf" "$OUT/test_syscall.o"
objcopy -O binary "$OUT/user.elf" "$OUT/user.bin"

debugfs -w -R "rm /user_program.bin" data.img 2>/dev/null || true
debugfs -w -R "write $OUT/user.bin user_program.bin" data.img

echo "user_program.bin installed to data.img"
