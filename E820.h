#pragma once
#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "print_text.h"

bool isUsableEntry(const E820Entry* entry);
E820Info* init_E820(uintptr_t address);
E820Entry* get_E820_entries(uint32_t count, uint32_t address);
void print_memory_map(E820Info* info);
void print_memory_entry(E820Entry* entry, uint32_t i);
uint32_t num_valid_entries(E820Info* info);
uint32_t num_usable_entries(E820Info* info);
void fetch_usable_memory(E820Info* info, E820Entry* usable_entries);
void fetch_unusable_memory(E820Info* info, E820Entry* unusable_entries);
void print_E820_entrys(E820Entry* entries, uint32_t length);
void print_E820_entry(E820Entry entry, uint32_t idx);