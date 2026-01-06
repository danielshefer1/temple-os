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
        bins[bit].head_free = CreateBuddyNode((void*)start);
        start += 1 << bit;
    }
}

BuddyNode* CreateBuddyNode(void* address) {
    BuddyNode* node = (BuddyNode*) kmalloc(sizeof(BuddyNode));
    node->address = address;
    node->next = NULL;
    return node;
}

void PrintBuddyBin() {
    for (int i = 0; i < MAX_ORDER; i++) {
        if (bins[i].head_free == NULL) {
            kprintf("Order: %d Free:\n", i);
            PrintBuddyNode(bins[i].head_free);
        }
        if (bins[i].head_used == NULL) {
            kprintf("Order: %d Used:\n", i);
            PrintBuddyNode(bins[i].head_used);
        }

    }
}

void PrintBuddyNode(BuddyNode* node) {
    BuddyNode* p = node;
    while (p != NULL) {
        kprintf("Address: 0x%x\t", p->address);
        p = p->next;
    }
    kprintf("\n");
}