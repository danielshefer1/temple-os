#include <stdint.h>
#include "paging.h"
#include "data_structs.h"

__attribute__((section(".bootstrap")))
void bootstrap_kmain() {
    uint32_t pd_addr = page_dir_addr();
    pde_t* page_directory = (pde_t*) pd_addr;
    uint32_t kernel_pages = (uint32_t)&__total_pages;

    InitPageDirectory(page_directory, pd_addr, kernel_pages);
    pte_t* pt_513 = (pte_t*)(uint32_t)(page_directory[513].frame << 12);
}