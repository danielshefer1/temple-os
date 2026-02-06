 #include "kernel.h"

void kmain() {
    clear_screen();
    kprintf("Kernel Main Started!\n");
    SetGDT();
    kprintf("GDT Set!\n");
    InitIDT();
    kprintf("IDT Set!\n");
    enable_sse();
    kprintf("SSE Enabled!\n");
    InitPaging();
    kprintf("Paging Initialized!\n");
    CliHelper();
    HltHelper();


    start();

    
    end();
}