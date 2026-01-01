#include "kernel.h"

uint32_t page_dir_addr_v() {
    uint32_t kernel_pages = (uint32_t)&__total_pages;
    return KERNEL_VIRTUAL + KERNEL_BASE + kernel_pages * PAGE_SIZE;
}

void end() {
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}

void flush_tlb() {
    __asm__ volatile("mov %%cr3, %%eax; mov %%eax, %%cr3" ::: "eax");

}

void kmain() {
    clear_screen();

    uint32_t pd_addr = page_dir_addr_v();
    pde_t* page_directory = (pde_t*) pd_addr;
    page_directory[0].present = 0;
    flush_tlb();

    E820Info* memory_map = init_E820(E820_ADDRESS);
    uint32_t valid_entries = num_valid_entries(memory_map);
    uint32_t n_usable_entries = num_usable_entries(memory_map);
    E820Entry usable_entries[n_usable_entries], unusable_entries[valid_entries - n_usable_entries];
    fetch_usable_memory(memory_map, usable_entries);
    fetch_unusable_memory(memory_map, unusable_entries);

    BuddyList* buddy_list = InitBuddyList(4, &usable_entries[0]);
    PrintBuddyList(buddy_list);
    end();
}