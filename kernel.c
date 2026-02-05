 #include "kernel.h"

void kmain() {
    clear_screen();
    kprintf("Kernel Main Started!\n");
    SetGDT();
    kprintf("GDT Set!\n");

    CliHelper();
    HltHelper();


    start();

    e820_info_t* info = init_E820(E820_ADDRESS);

    
    end();
}