# Bootloader build targets
.PHONY: all clean run

all: disk.img

boot.bin: boot.asm
    nasm -f bin boot.asm -o boot.bin

boot2.bin: boot2.asm
    nasm -f bin boot2.asm -o boot2.bin

disk.img: boot.bin boot2.bin
    cat boot.bin boot2.bin > disk.img

run: disk.img
    qemu-system-x86_64 -drive format=raw,file=disk.img

clean:
    rm -f boot.bin boot2.bin disk.img

.PHONY: all clean run