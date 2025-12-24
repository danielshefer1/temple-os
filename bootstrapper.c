#include <stdint.h>
#include "E820.h"
#include "paging.h"
#include "data_structs.h"

__attribute__((section(".bootstrap")))
void bootstrap_kmain() {
    E820Info* memory_map = init_E820(E820_ADDRESS);
    uint32_t usable_entries = num_usable_entries(memory_map);
    E820Entry unusable_entries[memory_map->num_entries - usable_entries - 1];
    fetch_unusable_memory(memory_map, unusable_entries);

    uint32_t pd_addr = page_dir_addr();
    pde_t* page_directory = (pde_t*) pd_addr;
    uint32_t kernel_pages = (uint32_t)&__total_pages;

    uint32_t init_count = InitPageDirectory(page_directory, pd_addr, kernel_pages);
    pte_t init_table[init_count];

    pte_t* table = (pte_t*)((uint32_t)pd_addr + PAGE_SIZE);

    for (uint32_t i = 0, idx = 0; i < 1024; i++) {
        if (table[i].present) {
            init_table[idx] = table[i];
            idx++;
        }
    }

}