 #include "kernel.h"

void kmain() {
    InitPaging();
    //kprintf("Paging Initialized!\n");
    clear_screen();
    //kprintf("Kernel Main Started!\n");
    SetGDT();
    //kprintf("GDT Set!\n");
    InitIDT();
    //kprintf("IDT Set!\n");
    enable_sse();
    //kprintf("SSE Enabled!\n");
    InitSlabAlloc((uint64_t)&(_stack_top));
    //kprintf("Slab Allocator Initialized!\n");
    e820_info_t* info = init_E820(E820_ADDRESS);
    if (info == NULL) {
        //kerror("Failed to initialize E820!");
    }
    InitBuddyAlloc(info);
    //kprintf("Buddy Allocator Initialized!\n");

    InitRsdt();
    //kprintf("RSDT Initialized!\n");
    InitMadt();
    //kprintf("MADT Initialized!\n");
    InitMcfg();
    clear_screen();

    InitConsoleBuffer();

    EnableLapic();
    
    InitTimer(TIMER_TICK_PER_MS);
    InitKeyboard();


    CliHelper();
    HltHelper();


    start();

    
    end();
}