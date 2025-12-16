#include <stdint.h>
#include <stdarg.h>
#include "print_text.h"
#include "E820.h"
#include "paging.h"

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
}