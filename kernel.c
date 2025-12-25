#include "kernel.h"

void kmain() {
    //clear_screen();
    uint32_t test = 0x8654;
    *((uint16_t*)0xB8000) = (uint16_t)'X' | (uint16_t)0x0F << 8;
    print_str("xdddd");
}