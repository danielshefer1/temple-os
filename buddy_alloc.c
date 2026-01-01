#include "buddy_alloc.h"

static uint32_t start_page = 0;
static uint32_t curr_id = 0;
static uint32_t curr_addr = 0;

BuddyList* InitBuddyList(uint32_t stack_size, E820Entry* entry) {
    uint32_t kernel_pages = (uint32_t)&__total_pages;
    uint32_t heap_addr = KERNEL_VIRTUAL + KERNEL_BASE + kernel_pages * PAGE_SIZE +
        3 * PAGE_SIZE + stack_size * PAGE_SIZE;
    start_page = (heap_addr - KERNEL_VIRTUAL) / PAGE_SIZE + 1;
    
    BuddyList* list = (BuddyList*) heap_addr;
    uint32_t bit_pos = 32, bit;
    while (bit_pos > 0) {
        bit = entry->length_low >> (bit_pos - 1);
        if (bit == 1) {
            break;
        }
        bit_pos--;
    }
    list->size = bit_pos - 1;
    list->next = NULL;
    list->prev = NULL;

    curr_addr = (heap_addr + sizeof(BuddyList));
    list->head = (buddyNode*) curr_addr;
    curr_addr += sizeof(buddyNode);
    list->head->free = 1;
    list->head->next = NULL;
    list->head->prev = NULL;
    list->head->id = curr_id++;
    list->head->buddy = 0;
    if (entry->base_low <= heap_addr - KERNEL_VIRTUAL) {
        list->head->start = heap_addr - KERNEL_VIRTUAL;
    }
    else {
        list->head->start = entry->base_low;
    }
    return list;
}

void PrintBuddyListH(BuddyList* list) {
    if (list == NULL) {
        return;
    }
    kprintf("Size: %d\n", list->size);
    PrintBuddyNode(list->head);
    PrintBuddyListH(list->next);
}

void PrintBuddyList(BuddyList* list) {
    BuddyList* start = list;
    while (start->prev != NULL) {
        start = start->prev;
    }
    PrintBuddyListH(start);
}

void PrintBuddyNodeH(buddyNode* node) {
    if (node == NULL) {
        return;
    }
    kprintf("Free: %d\t", node->free);
    kprintf("ID: %d\t", node->id);
    kprintf("Buddy: %d\t", node->buddy);
    kprintf("Start: 0x%x\n", node->start);

    if (node->next != NULL) {
        PrintBuddyNode(node->next);
    }
}

void PrintBuddyNode(buddyNode* node) {
    buddyNode* start = node;
    while (start->prev != NULL) {
        start = start->prev;
    }
    PrintBuddyNodeH(start);
}

void AddKernelPages(pte_t *page_table, uint32_t count) {
    for (uint32_t i = start_page; i < start_page + count; i++) {
        page_table[i].present = 1;
        page_table[i].rw = 1;
        page_table[i].user = 0;
        page_table[i].write_thru = 0;
        page_table[i].cache_dis = 0;
        page_table[i].accessed = 0;
        page_table[i].dirty = 0;
        page_table[i].pat = 0;
        page_table[i].global = 1;
        page_table[i].frame = (i * PAGE_SIZE) >> 12;
    }
    start_page += count;
}


void* brk(BuddyList* list, uint32_t size) {
    BuddyList* p = list;
    uint32_t bit_pos = 32, bit;

    while (bit_pos > 0) {
        bit = size >> (bit_pos - 1);
        if (bit == 1) {
            break;
        }
        bit_pos--;
    }
    size = bit_pos;
    while (p->size <= size) {
        if (p->size == size) {
            while (p->head->free == 0) {
                if (p->head->next == NULL) {
                    break;
                }
                p->head = p->head->next;
            }
            if (p->head->free == 1) {
                p->head->free = 0;
                return (void*)p->head->start;
            }
        }
        p = p->next;
    }
    while (p != NULL) {
        while (p->head->free == 0) {
            if (p->head->next == NULL) {
                break;
            }
            p->head = p->head->next;
        }
        if (p->head->free == 1) {
            
        }
    }

    return NULL;

}
