#!/bin/sh

K_ENTRY=$(nm kernel64.elf | grep " kmain" | awk '{print "0x"$1}')
K_SECTORS=$(nm kernel64.elf | grep " __kernel_sectors" | awk '{print "0x"$1}')
BSS_START=$(nm kernel64.elf | grep " _bss_start" | awk '{print "0x"$1}')
BSS_END=$(nm kernel64.elf | grep " _bss_end" | awk '{print "0x"$1}')


printf "KERNEL_ENTRY_64 equ %s\n" "$K_ENTRY" > offsets.inc
printf "KERNEL_SECTORS equ %s\n" "$K_SECTORS" >> offsets.inc
printf "KERNEL_BSS_START equ %s\n" "$BSS_START" >> offsets.inc
printf "KERNEL_BSS_END equ %s\n" "$BSS_END" >> offsets.inc