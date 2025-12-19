#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "print_text.h"
#include "E820.h"
#include "paging.h"
#include "data_structs.h"

void kmain() {
    clear_screen();
    
    E820Info *e820_info = init_E820((uintptr_t)E820_ADDRESS);
    print_memory_map(e820_info);

    uint32_t usable_count = num_usable_entries(e820_info);
    kprintf("Number of Usable Memory Entries: %d\n", usable_count);
    E820Entry usable_entries[usable_count];

    E820Entry* usable_memory = fetch_usable_memory(e820_info, usable_entries);
    kprintf("Usable Memory Entries:\n");
    for (size_t i = 0; i < num_usable_entries(e820_info); i++) {
        print_memory_entry(&usable_memory[i], i);
    }

    const uint32_t kernel_pages = (uint32_t)&__total_pages;

    kprintf("total kernel pages: %d\n", kernel_pages);

    const uint32_t pd_addr = page_dir_addr();
    kprintf("Page Directory Address: 0x%x\n", pd_addr);

    /* pte_t* page_directory = (pte_t*)pd_addr;
    fill_page_directory(page_directory, usable_memory, kernel_pages);
    kprintf("Page directory initialized.\n"); */
}