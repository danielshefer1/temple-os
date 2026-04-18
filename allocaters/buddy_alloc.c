#include "buddy_alloc.h"

static buddy_bin_t user_bins[MAX_ORDER];
static buddy_bin_t kernel_bins[MAX_ORDER];
static uint64_t user_lowest_vaild = UINT64_MAX;
static uint64_t kernel_lowest_vaild = UINT64_MAX;

void AddToBuddyAlloc(uint64_t start, uint64_t size, bool user) {
    uint64_t bit, start_order, size_order;
    buddy_bin_t* bins = user ? user_bins : kernel_bins;
    uint64_t* lowest_valid = user ? &user_lowest_vaild : &kernel_lowest_vaild;

    while (size >= PAGE_SIZE) {
        start_order = SmallestBit(start);
        size_order = BiggestBit(size);
        bit = (start_order < size_order) ? start_order : size_order;

        if (bit < PAGE_SIZE_LOG2) {
            start += 1ULL << bit;
            size -= 1ULL << bit;
            continue;
        };
        size -= 1ULL << bit;
        buddy_node_t* node = CreateBuddyNode((void*)start, bit);
        InsertSortedBuddyNode(&bins[bit], node, true);
        start += 1ULL << bit;
        if (bit < *lowest_valid) {*lowest_valid = bit;}
    }
}

void InitUserBuddyAlloc(e820_info_t* info) {
    uint64_t base, length;
    for (uint64_t i = 0; i < info->num_entries; i++) {
        base = ((uint64_t)info->entries[i].base_high << 32) | info->entries[i].base_low;
        length = ((uint64_t)info->entries[i].length_high << 32) | info->entries[i].length_low;

        if (base == MB) {
            if (length < GB - MB) continue;
            AddToBuddyAlloc(GB, length - GB + MB, true);
            continue;
        };

        if (info->entries[i].type == 1 && length >= MB) {
            AddToBuddyAlloc(base, length, true);
        }
    }
}

void InitKernelBuddyAlloc(uint64_t start, uint64_t end) {
    if (end <= start) return;
    AddToBuddyAlloc(start, end - start, false);
}

buddy_node_t* CreateBuddyNode(void* address, uint64_t order) {
    buddy_node_t* node = (buddy_node_t*) kmalloc(sizeof(buddy_node_t));
    node->free = true;
    node->address = address;
    node->order = order;
    node->next = NULL;
    return node;
}

buddy_node_t* CreateDupeBuddyNode(buddy_node_t* original) {
    buddy_node_t* node = (buddy_node_t*) kmalloc(sizeof(buddy_node_t));
    node->free = original->free;
    node->address = original->address;
    node->order = original->order;
    node->next = NULL;
    return node;
}

void RemoveBuddyNode(buddy_bin_t* bin, void* address, bool free_list) {
    buddy_node_t** head = free_list ? &bin->head_free : &bin->head_used;
    buddy_node_t* current = *head;
    buddy_node_t* prev = NULL;

    while (current != NULL) {
        if (current->address == address) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            kfree(current, sizeof(buddy_node_t));
            return;
        }
        prev = current;
        current = current->next;
    }
}

void RemoveBuddyNodeComplete(buddy_bin_t* bin, void* address) {
    RemoveBuddyNode(bin, address, true);
    RemoveBuddyNode(bin, address, false);
}

void MoveBuddyNode(buddy_bin_t* bin, buddy_node_t* node) {
    bool was_free = node->free;
    node->free = !node->free;
    buddy_node_t* new_node = CreateDupeBuddyNode(node);
    InsertSortedBuddyNode(bin, new_node, !was_free);
    RemoveBuddyNode(bin, node->address, was_free);
}

void* SplitNode(buddy_node_t* node, uint64_t target_order, buddy_bin_t* bins) {
    if (node == NULL) {
        return NULL;
    }
    if (node->order < target_order || !node->free) {
        return NULL;
    }
    if (node->order == target_order) {
        void* ret = node->address;
        MoveBuddyNode(&bins[node->order], node);
        return ret;
    }
    uint64_t addr = (uint64_t)node->address, curr_order = node->order;
    RemoveBuddyNode(&bins[node->order], node->address, true);

    buddy_node_t* target_buddy1 = CreateBuddyNode((void*)addr, target_order);
    target_buddy1->free = false;
    InsertSortedBuddyNode(&bins[target_order], target_buddy1, false);

    while (curr_order > target_order) {
        void* buddy_address = GetBuddyAddress((void*)addr, curr_order - 1);
        buddy_node_t* buddy2 = CreateBuddyNode(buddy_address, curr_order - 1);
        InsertSortedBuddyNode(&bins[curr_order - 1], buddy2, true);
        curr_order--;
    }
    return target_buddy1->address;
}

bool MergeBuddy(void* address, uint64_t order, buddy_bin_t* bins) {
    void* buddy_address = GetBuddyAddress(address, order);
    buddy_node_t* buddy_node = bins[order].head_free;
    bool found = false;

    while (buddy_node != NULL) {
        if (buddy_node->address == buddy_address) {
            found = true;
            break;
        }
        buddy_node = buddy_node->next;
    }

    if (found) {
        RemoveBuddyNodeComplete(&bins[order], address);
        RemoveBuddyNodeComplete(&bins[order], buddy_address);

        void* merged_address = (address < buddy_address) ? address : buddy_address;
        buddy_node_t* merged_node = CreateBuddyNode(merged_address, order + 1);
        merged_node->free = false;
        InsertSortedBuddyNode(&bins[order + 1], merged_node, false);

        bool merged = MergeBuddy(merged_address, order + 1, bins);
        if (!merged) {
            MoveBuddyNode(&bins[order + 1], merged_node);
        }
    }
    return found;
}

buddy_node_t* FindBuddyNode(buddy_bin_t* bin, void* address) {
    buddy_node_t* current_free = bin->head_free, *current_used = bin->head_used;
    while (current_free != NULL || current_used != NULL) {
        if (current_free != NULL && current_free->address == address) {
            return current_free;
        }
        if (current_used != NULL && current_used->address == address) {
            return current_used;
        }
        if (current_free != NULL) {
            current_free = current_free->next;
        }
        if (current_used != NULL) {
            current_used = current_used->next;
        }
    }
    return NULL;
}

void FreeBuddy(void* address, bool user) {
    buddy_bin_t* bins = user ? user_bins : kernel_bins;
    uint64_t* lowest_valid_p = user ? &user_lowest_vaild : &kernel_lowest_vaild;

    uint64_t order = 0, page_count;
    buddy_node_t* node = FindBuddyNode(&bins[order], address);

    while (node == NULL && order < MAX_ORDER) {
        order++;
        node = FindBuddyNode(&bins[order], address);
    }
    if (node == NULL) {
        return;
    }
    bool org_int_state = check_interrupts();
    CliHelper();
    if (1 << node->order < TABLE_SIZE) {
        page_count = (1 << node->order) / PAGE_SIZE;
        if ((uint64_t)(1 << node->order) % PAGE_SIZE != 0) {
            page_count++;
        }
    }

    bool merged = MergeBuddy(address, order, bins);
    if (!merged) {
        MoveBuddyNode(&bins[order], node);
    }
    if (org_int_state) StiHelper();

    *lowest_valid_p = FindLowest(bins);

}

void* RequestBuddy(uint64_t size, bool user) {
    buddy_bin_t* bins = user ? user_bins : kernel_bins;
    uint64_t* lowest_vaild_p = user ? &user_lowest_vaild : &kernel_lowest_vaild;

    uint64_t order = BiggestBit(size), tmp;
    if (!IsPowerOfTwo(size)) order++;

    if (order < PAGE_SIZE_LOG2 + 1) {
        order = PAGE_SIZE_LOG2;
    }

    if (order >= MAX_ORDER) {
        return NULL;
    }

    if (order < *lowest_vaild_p) {
        tmp = order;
        order = *lowest_vaild_p;
        *lowest_vaild_p = tmp;
    } 


    for (uint64_t current_order = order; current_order < MAX_ORDER; current_order++) {
        if (bins[current_order].head_free != NULL) {
            bool org_int_state = check_interrupts();
            CliHelper();

            void* ret = SplitNode(bins[current_order].head_free, order, bins);
            if (ret != NULL) {
                if (org_int_state) StiHelper();
                return ret;
            }
        }
    }
    return NULL;
}

void* GetBuddyAddress(void* address, uint64_t order) {
    uint64_t addr = (uint64_t)address;
    uint64_t buddy_addr = addr ^ (1 << (order - 1));
    return (void*)buddy_addr;
}

void InsertSortedBuddyNode(buddy_bin_t* bin, buddy_node_t* node, bool free_list) {
    buddy_node_t** head = free_list ? &bin->head_free : &bin->head_used;

    if (*head == NULL || (*head)->address > node->address) {
        node->next = *head;
        *head = node;
        return;
    }
    buddy_node_t* current = *head;
    while (current->next != NULL && current->next->address < node->address) {
        current = current->next;
    }
    node->next = current->next;
    current->next = node;
}

uint64_t FindLowest(buddy_bin_t* bins) {
    for (uint64_t i = PAGE_SIZE_LOG2; i < MAX_ORDER; i++) {
        if (bins[i].head_free != NULL) return i;
    }
    return 0;
}



void PrintBuddyBin(uint64_t start_order, uint64_t end_order, bool user) {
    buddy_bin_t* bins = user ? user_bins : kernel_bins;

    bool sti = check_interrupts();
    CliHelper();
    for (uint64_t i = start_order; i < end_order && i < MAX_ORDER; i++) {
        if (bins[i].head_free == NULL && bins[i].head_used == NULL) {
            continue;
        }
        kprintf("Order: %d\n", i);

        if (bins[i].head_free != NULL) {
            kprintf("Free: ");
            PrintBuddyNode(bins[i].head_free);
        }
        if (bins[i].head_used != NULL) {
            kprintf("Used: ");
            PrintBuddyNode(bins[i].head_used);
        }

    }
    if (sti) StiHelper();
}

void PrintBuddyNode(buddy_node_t* node) {
    buddy_node_t* p = node;
    while (p->next != NULL) {
        kprintf("Address: %x -> ", p->address);
        p = p->next;
    }
    kprintf("Address: %x\n", p->address);
}