 #include "kernel.h"

void kmain() {
    InitPaging();
    kprintf("Paging Initialized!\n");
    clear_screen();
    kprintf("Kernel Main Started!\n");
    SetGDT();
    kprintf("GDT Set!\n");
    InitIDT();
    kprintf("IDT Set!\n");
    enable_sse();
    kprintf("SSE Enabled!\n");
    InitSlabAlloc((uint64_t)&(_stack_top));
    kprintf("Slab Allocator Initialized!\n");
    InitRsdt();
    kprintf("RSDT Initialized!\n");
    InitMadt();
    kprintf("MADT Initialized!\n");


    CliHelper();
    HltHelper();


    start();

    
    end();
}