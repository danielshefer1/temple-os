#pragma once
#include <stdint.h>
#include <stddef.h>
#include "print_text.h"
#include "E820.h"
#include "data_structs.h"
extern uint32_t __total_pages;
extern uint32_t __total_sectors;
extern uint32_t _kernel_VMA_start;
#define PAGE_SIZE 4096
static uint32_t KERNEL_BASE = (uint32_t)&(_kernel_VMA_start);

typedef struct PageTableEntry {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} PageTableEntry;

uint32_t num_usable_entries(E820Info* info);
E820Entry* fetch_usable_memory(E820Info* info, E820Entry* usable_entries);

uint32_t num_usable_entries(E820Info* info) {
    uint32_t count = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type != 1) {
            continue;
        }
        if (info->entries[i].length_low < 1024 * 4 && info->entries[i].length_high != 0) {
            continue;
        }
        if (info->entries[i].base_high != 0 || info->entries[i].base_low == 0) {
            continue;
        }
        count++;
    }
    return count;
}

E820Entry* fetch_usable_memory(E820Info* info, E820Entry* usable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type != 1) {
            continue;
        }
        if (info->entries[i].length_low < 1024 * 4 && info->entries[i].length_high != 0) {
            continue;
        }
        if (info->entries[i].base_high != 0 || info->entries[i].base_low == 0) {
            continue;
        }
        usable_entries[index++] = info->entries[i];
    }

    return usable_entries;
}

uint32_t page_dir_addr() {
    uint32_t total_pages = (uint32_t)&__total_pages;
    return KERNEL_BASE + (total_pages * PAGE_SIZE);
}

void fill_page_directory(PageTableEntry* page_directory, E820Info* e820_info, E820Entry* usable_memory, uint32_t kernel_pages) {
    uint32_t pd_addr = page_dir_addr();
    for (size_t i = 0; i < 1024; i++) {
        if ((i * PAGE_SIZE >= KERNEL_BASE && i * PAGE_SIZE < (KERNEL_BASE + (kernel_pages * PAGE_SIZE))) || (i * PAGE_SIZE == pd_addr)) {
            page_directory[i].present = 1;
            page_directory[i].rw = 1;
            page_directory[i].user = 0;
            page_directory[i].frame = (i * PAGE_SIZE) >> 12;
            page_directory[i].avail = 0;
            page_directory[i].global = 1;
            page_directory[i].write_thru = 0;
            page_directory[i].cache_dis = 0;
            page_directory[i].accessed = 0;
            page_directory[i].dirty = 0;
            page_directory[i].pat = 0;
            continue;
        }
        page_directory[i].present = 0;
    }
}

void print_present_pages(PageTableEntry* page_directory) {
    kprintf("Present Pages in Page Directory:\n");
    for (size_t i = 0; i < 1024; i++) {
        if (page_directory[i].present) {
            kprintf("Page Directory Entry %d: Frame Address: 0x%x\n", i, page_directory[i].frame << 12);
        }
    }
}