#include "buddy_alloc.h"

static BuddyBin bins[MAX_ORDER];

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
    }
}

BuddyNode* CreateBuddyNode(void* address, uint32_t order) {
    BuddyNode* node = (BuddyNode*) kmalloc(sizeof(BuddyNode));
    node->free = true;
    node->address = address;
    node->order = order;
    node->next = NULL;
    return node;
}

BuddyNode* CreateDupeBuddyNode(BuddyNode* original) {
    BuddyNode* node = (BuddyNode*) kmalloc(sizeof(BuddyNode));
    node->free = original->free;
    node->address = original->address;
    node->order = original->order;
    node->next = NULL;
    return node;
}

void RemoveBuddyNode(BuddyBin* bin, void* address, bool free_list) {
    BuddyNode** head = free_list ? &bin->head_free : &bin->head_used;
    BuddyNode* current = *head;
    BuddyNode* prev = NULL;

    while (current != NULL) {
        if (current->address == address) {
            if (prev == NULL) {
                *head = current->next;
            } else {
                prev->next = current->next;
            }
            kfree(current, sizeof(BuddyNode));
            return;
        }
        prev = current;
        current = current->next;
    }
}

void RemoveBuddyNodeComplete(BuddyBin* bin, void* address) {
    RemoveBuddyNode(bin, address, true);
    RemoveBuddyNode(bin, address, false);
}

void MoveBuddyNode(BuddyBin* bin, BuddyNode* node) {
    bool was_free = node->free;
    node->free = !node->free;
    BuddyNode* new_node = CreateDupeBuddyNode(node);
    InsertSortedBuddyNode(bin, new_node, !was_free);
    RemoveBuddyNode(bin, node->address, was_free);
}

void* SplitNode(BuddyNode* node, uint32_t target_order) {
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

    BuddyNode* target_buddy1 = CreateBuddyNode((void*)addr, target_order);
    target_buddy1->free = false;
    InsertSortedBuddyNode(&bins[target_order], target_buddy1, false);

    while (curr_order > target_order) {
        void* buddy_address = GetBuddyAddress((void*)addr, curr_order - 1);
        BuddyNode* buddy2 = CreateBuddyNode(buddy_address, curr_order - 1);
        InsertSortedBuddyNode(&bins[curr_order - 1], buddy2, true);
        curr_order--;
    }
    return target_buddy1->address;
}

bool MergeBuddy(void* address, uint32_t order) {
    void* buddy_address = GetBuddyAddress(address, order);
    BuddyNode* buddy_node = bins[order].head_free;
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
        BuddyNode* merged_node = CreateBuddyNode(merged_address, order + 1);
        merged_node->free = false;
        InsertSortedBuddyNode(&bins[order + 1], merged_node, false);

        bool merged = MergeBuddy(merged_address, order + 1);
        if (!merged) {
            MoveBuddyNode(&bins[order + 1], merged_node);
        }
    }
    return found;
}

BuddyNode* FindBuddyNode(BuddyBin* bin, void* address) {
    BuddyNode* current_free = bin->head_free, *current_used = bin->head_used;
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
    uint32_t order = 0;
    BuddyNode* node = FindBuddyNode(&bins[order], address);
    while (node == NULL && order < MAX_ORDER) {
        order++;
        node = FindBuddyNode(&bins[order], address);
    }
    if (node == NULL) {
        return;
    }
    bool merged = MergeBuddy(address, order);
    if (!merged) {
        MoveBuddyNode(&bins[order], node);
    }

}

void* RequestBuddy(uint32_t size) {
    uint32_t order = BiggestBit(size);
    if (!IsPowerOfTwo(size)) order++;

    if (order >= MAX_ORDER) {
        return NULL;
    }

    for (uint32_t current_order = order; current_order < MAX_ORDER; current_order++) {
        if (bins[current_order].head_free != NULL) {
            return SplitNode(bins[current_order].head_free, order);
        }
    }
    return NULL;
}

void* GetBuddyAddress(void* address, uint32_t order) {
    uint32_t addr = (uint32_t)address;
    uint32_t buddy_addr = addr ^ (1 << (order - 1));
    return (void*)buddy_addr;
}

void InsertSortedBuddyNode(BuddyBin* bin, BuddyNode* node, bool free_list) {
    BuddyNode** head = free_list ? &bin->head_free : &bin->head_used;

    if (*head == NULL || (*head)->address > node->address) {
        node->next = *head;
        *head = node;
        return;
    }
    BuddyNode* current = *head;
    while (current->next != NULL && current->next->address < node->address) {
        current = current->next;
    }
    node->next = current->next;
    current->next = node;
}



void PrintBuddyBin(uint32_t start_order, uint32_t end_order) {
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
}

void PrintBuddyNode(BuddyNode* node) {
    BuddyNode* p = node;
    while (p->next != NULL) {
        kprintf("Address: 0x%x -> ", p->address);
        p = p->next;
    }
    kprintf("Address: 0x%x\n", p->address);
}