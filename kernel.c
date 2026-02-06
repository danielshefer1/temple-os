 #include "kernel.h"

void kmain() {
    clear_screen();
    kprintf("Kernel Main Started!\n");
    SetGDT();
    kprintf("GDT Set!\n");
    InitIDT();
    kprintf("IDT Set!\n");
    uint64_t test = 5 / 0;

    CliHelper();
    HltHelper();


    start();

    
    end();
}