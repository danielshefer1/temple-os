#include "includes.h"
#include "paging_bootstrap.h"
#include "types.h"
#include "kernel.h"

__attribute__((section(".bootstrap")))
void bootstrap_kmain() {
    uint32_t pd_addr = page_dir_addr();
    pde_t* page_directory = (pde_t*) pd_addr;
    uint32_t kernel_pages = (uint32_t)&__total_pages;
    uint32_t text_size = (uint32_t)&_text_size;

    InitPageDirectory(page_directory, pd_addr, kernel_pages, text_size);

    enable_paging((uint32_t*) page_directory);
}