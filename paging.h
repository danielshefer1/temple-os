#include <stdint.h>
#include <stddef.h>
#include "print_text.h"
#include "E820.h"


uint32_t num_usable_entries(E820Info* info);
E820Entry* fetch_usable_memory(E820Info* info, E820Entry* usable_entries);

uint32_t num_usable_entries(E820Info* info) {
    uint32_t count = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type == 1 && (info->entries[i].length_low > 1024 * 4 && info->entries[i].length_high == 0)
         && info->entries[i].base_high == 0) { // Type 1 indicates usable RAM
            count++;
        }
    }
    return count;
}

E820Entry* fetch_usable_memory(E820Info* info, E820Entry* usable_entries) {
    uint32_t index = 0;
    for (size_t i = 0; i < info->num_entries; ++i) {
        if (info->entries[i].type == 1 && (info->entries[i].length_low > 1024 * 4 && info->entries[i].length_high == 0)
         && info->entries[i].base_high == 0) {
            usable_entries[index++] = info->entries[i];
        }
    }

    return usable_entries;
}