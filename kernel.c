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
    //clear_screen();

    //kprintf("If this worked I'm gonna scream\n");
    //kprintf("Number example: 0x%x\n", 0x8765);
    //E820Info* memory_map = init_E820(E820_ADDRESS);


    uint32_t pd_addr = page_dir_addr_v();
    //kprintf("Page Directory Virtual Address: 0x%x\n", pd_addr);
    pde_t* page_directory = (pde_t*) pd_addr;
    //kprintf("PDE[0] global bit: %d\n", page_directory[0].global);
    //kprintf("Is the first page table present: %d\n", page_directory[0].present);
    page_directory[0].present = 0;
    flush_tlb();
    clear_screen();
    kprintf("Is the first page table present: %d\n", page_directory[0].present);
    uint32_t kernel_table_addr = page_directory[512].frame << 12;
    kernel_table_addr += KERNEL_VIRTUAL;
    kprintf("Kernel table addr: 0x%x\n", kernel_table_addr);

    pte_t* kernel_table = (pte_t*) kernel_table_addr;
    kprintf("Is the first page table entry present: %d\n", kernel_table[0].present);
    kprintf("E820 Address: 0x%x\n", E820_ADDRESS);
    kprintf("VGA Buffer Address: 0x%x", VGA_BUFFER);

    pte_t* higher_half_table = (pte_t*)(pd_addr + PAGE_SIZE * 2); // 0x80104000

    kprintf("Higher-half table[0] - present:%d frame:0x%x\n", 
        higher_half_table[0].present, higher_half_table[0].frame);
    
    //uint32_t num_usable = num_usable_entries(memory_map);
    /*
    E820Entry usable_entries[num_usable], unusable_entries[memory_map->num_entries - 1 - num_usable];

    fetch_unusable_memory(memory_map, unusable_entries);
    fetch_usable_memory(memory_map, usable_entries);

    kprintf("Unusable Entries:\n");
    print_E820_entrys(unusable_entries, memory_map->num_entries - 1 - num_usable);
    kprintf("Usable Entries:\n");
    print_E820_entrys(unusable_entries, num_usable);
    */
    end();
}