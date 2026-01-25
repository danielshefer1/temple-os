#include "buddy_alloc.h"

static buddy_bin_t bins[MAX_ORDER];
static uint32_t lowest_valid;

void InitBuddyAlloc(uint32_t start, uint32_t size) {
    for (int i = 0; i < MAX_ORDER; i++) {
        bins[i].head_free = NULL;
        bins[i].head_used = NULL;
    }
    uint32_t bit;

    while (size > 0) {
        bit = BiggestBit(size);
        size -= 1 << bit;
        bins[bit].head_free = CreateBuddyNode((void*)start, bit);
        start += 1 << bit;
        lowest_valid = bit;
    }
}

buddy_node_t* CreateBuddyNode(void* address, uint32_t order) {
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

void* SplitNode(buddy_node_t* node, uint32_t target_order) {
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
    uint32_t addr = (uint32_t)node->address, curr_order = node->order;
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

bool MergeBuddy(void* address, uint32_t order) {
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

        bool merged = MergeBuddy(merged_address, order + 1);
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

void FreeBuddy(void* address) {
    uint32_t order = 0, page_count;
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
        if ((uint32_t)(1 << node->order) % PAGE_SIZE != 0) {
            page_count++;
        }
        RemovePages(((uint32_t)address - USER_BASE) >> 21, (uint32_t)address % TABLE_SIZE / PAGE_SIZE, page_count);
    }
    else {
        RemovePageTables(((uint32_t)address - USER_BASE) >> 21, ((uint32_t)address + (1 << order) - USER_BASE) >> 21);
    }

    bool merged = MergeBuddy(address, order);
    if (!merged) {
        MoveBuddyNode(&bins[order], node);
    }
    if (org_int_state) StiHelper();

    lowest_valid = FindLowest();

}

void* RequestBuddy(uint32_t size) {
    uint32_t order = BiggestBit(size), tmp;
    if (!IsPowerOfTwo(size)) order++;

    if (order < PAGE_SIZE_LOG2) {
        order = PAGE_SIZE_LOG2;
    }

    if (order >= MAX_ORDER) {
        return NULL;
    }

    if (order < lowest_valid) {
        tmp = order;
        order = lowest_valid;
        lowest_valid = tmp;
    } 


    for (uint32_t current_order = order; current_order < MAX_ORDER; current_order++) {
        if (bins[current_order].head_free != NULL) {
            bool org_int_state = check_interrupts();
            CliHelper();

            void* ret = SplitNode(bins[current_order].head_free, order);
            if (ret != NULL) {
                FillPageDirectoryUser(ret, (1 << order));
                if (org_int_state) StiHelper();
                return ret;
            }
        }
    }
    return NULL;
}

void* GetBuddyAddress(void* address, uint32_t order) {
    uint32_t addr = (uint32_t)address;
    uint32_t buddy_addr = addr ^ (1 << (order - 1));
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

uint32_t FindLowest() {
    for (uint32_t i = PAGE_SIZE_LOG2; i < MAX_ORDER; i++) {
        if (bins[i].head_free != NULL) return i;
    }
    return 0;
}



void PrintBuddyBin(uint32_t start_order, uint32_t end_order) {
    bool sti = check_interrupts();
    CliHelper();
    for (uint32_t i = start_order; i < end_order && i < MAX_ORDER; i++) {
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