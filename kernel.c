#include "kernel.h"



void end() {
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}



void kmain() {
    clear_screen();

    InitPaging();

    E820Info* memory_map = init_E820(E820_ADDRESS);
    uint32_t n_usable_entries = num_usable_entries(memory_map);
    E820Entry usable_entries[n_usable_entries];
    fetch_usable_memory(memory_map, usable_entries);

    InitSlabCache(PageDirAddrV() + 7 * PAGE_SIZE);
    InitBuddyAlloc(KERNEL_VIRTUAL / 2, KERNEL_VIRTUAL);
    PrintList();
    end();
}