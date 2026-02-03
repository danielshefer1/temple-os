#!/bin/sh

K_ENTRY=$(nm ./build/kernel.elf | grep " kmain" | awk '{print "0x"$1}')
K_SECTORS=$(nm ./build/kernel.elf | grep " __kernel_sectors" | awk '{print "0x"$1}')
BSS_START=$(nm ./build/kernel.elf | grep " _bss_start" | awk '{print "0x"$1}')
BSS_END=$(nm ./build/kernel.elf | grep " _bss_end" | awk '{print "0x"$1}')


printf "KERNEL_ENTRY_64 equ %s\n" "$K_ENTRY" > ./build/offsets.inc
printf "KERNEL_SECTORS equ %s\n" "$K_SECTORS" >> ./build/offsets.inc
printf "KERNEL_BSS_START equ %s\n" "$BSS_START" >> ./build/offsets.inc
printf "KERNEL_BSS_END equ %s\n" "$BSS_END" >> ./build/offsets.inc