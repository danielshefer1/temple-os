#pragma once
#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"

bool isUsableEntry(const e820_entry_t* entry);
e820_info_t* init_E820(uintptr_t address);
e820_entry_t* get_E820_entries(uint32_t count, uint32_t address);
void print_memory_map(e820_info_t* info);
void print_memory_entry(e820_entry_t* entry, uint32_t i);
uint32_t num_valid_entries(e820_info_t* info);
uint32_t num_usable_entries(e820_info_t* info);
void fetch_usable_memory(e820_info_t* info, e820_entry_t* usable_entries);
void fetch_unusable_memory(e820_info_t* info, e820_entry_t* unusable_entries);
void print_E820_entrys(e820_entry_t* entries, uint32_t length);
void print_E820_entry(e820_entry_t entry, uint32_t idx);