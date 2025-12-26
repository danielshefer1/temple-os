#include "kernel.h"

void kmain() {
    uint32_t test = 0x10000000;

    clear_screen();
    *((uint16_t*)0xB8000) = (uint16_t)'X' | (uint16_t)0x0F << 8;
    print_str("xdddd");
    while (test-- > 0);

    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}