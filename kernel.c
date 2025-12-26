#include "kernel.h"

uint32_t page_dir_addr_v() {
    uint32_t kernel_pages = (uint32_t)&__total_pages;
    return KERNEL_VIRTUAL + KERNEL_BASE + kernel_pages * PAGE_SIZE;
}

void kmain() {
    clear_screen();

    kprintf("If this worked I'm gonna scream\n");
    kprintf("Number example: 0x%x\n", 0x8765);

    uint32_t pd_addr = page_dir_addr_v();
    kprintf("Page Directory Virtual Address: 0x%x\n", pd_addr);
    pde_t* page_directory = (pde_t*) pd_addr;
    kprintf("Is the first page table present: %d\n", page_directory[0].present);
    page_directory[0].present = 0;
    kprintf("Is the first page table present: %d\n", page_directory[0].present);

    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}