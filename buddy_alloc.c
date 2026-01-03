#include "buddy_alloc.h"

static uint32_t curr_id = 0;

BuddyList* InitBuddyList(uint32_t start, uint32_t size) {
    BuddyList* list = AddList(), *tmp;
    list->next = NULL;

    uint32_t bit_pos;
    while (size > 0) {
        bit_pos = GetLargestBit(size);
        size -= 1 << (bit_pos);
        list->size = bit_pos;

        list->head = AddNode();
        list->head->free = 1;
        list->head->next = NULL;
        list->head->prev = NULL;
        list->head->id = curr_id++;
        list->head->buddy = 0;
        list->head->start = start;
        start += 1 << list->size;
        
        tmp = AddList();
        list->prev = tmp;
        tmp->next = list;
        list = tmp;
    }
    list = list->next;
    list->prev = NULL;
    return list;
}

uint32_t pow(uint32_t base, uint32_t exp) {
    uint32_t result = 1;
    while (exp > 0) {
        result *= base;
        exp--;
    }
    return result;
}

BuddyList* AddList() {
    return (BuddyList*) kmalloc(sizeof(BuddyList));
}
BuddyNode* AddNode() {
    return (BuddyNode*) kmalloc(sizeof(BuddyNode));
}

void PrintBuddyList(BuddyList* list) {
    BuddyList* start = list;
    while (start->prev != NULL) {
        start = start->prev;
    }
    while (start != NULL) {
        kprintf("Size: %d\n", start->size);
        PrintBuddyNode(start->head);
        start = start->next;
    }
}

void PrintBuddyNode(BuddyNode* node) {
    BuddyNode* start = node;
    while (start->prev != NULL) {
        start = start->prev;
    }
    while (start != NULL) {
        kprintf("Free: %d\t", start->free);
        kprintf("ID: %d\t", start->id);
        kprintf("Buddy: %d\t", start->buddy);
        kprintf("Start: 0x%x\n", start->start);
        start = start->next;
    } 
}
uint32_t GetLargestBit(uint32_t num) {
    uint32_t bit_pos = 32, bit;
    while (bit_pos > 0) {
        bit = num >> (bit_pos - 1);
        if (bit == 1) {
            break;
        }
        bit_pos--;
    }
    return bit_pos - 1;
}

uint32_t NumOfActiveBits(uint32_t num) {
    uint32_t bit_pos = 32, bit;
    uint32_t count = 0;
    while (bit_pos > 0) {
        bit = num >> (bit_pos - 1);
        if (bit == 1) {
            count++;
        }
        bit_pos--;
    }
    return count;
}

bool DeleteNode(BuddyNode* node, BuddyList* list) {
    if (node->next == NULL && node->prev == NULL) {
        list->head = NULL;
        return false;    
    }
    if (node->next != NULL && node->prev != NULL) {
        node->next->prev = node->prev;
        node->prev->next = node->next;
        return true;
    }
    if (node->next == NULL) {
        node->prev->next = NULL;
        return true;
    }
    if (node->prev == NULL) {
        node->next->prev = NULL;
        list->head = node->next;
        return true;
    }
    return false;
}

bool DeleteList(BuddyList* list) {
    if (list->next == NULL && list->prev == NULL) {
        return false;    
    }
    if (list->next != NULL && list->prev != NULL) {
        list->next->prev = list->prev;
        list->prev->next = list->next;
        return true;
    }
    if (list->next == NULL) {
        list->prev->next = NULL;
        return true;
    }
    if (list->prev == NULL) {
        list->next->prev = NULL;
        return true;
    }
    return false;
}

uint32_t SplitList(BuddyList* starting_list, uint32_t bit_pos, BuddyNode* split_node) {
    if (starting_list->size <= bit_pos) {
        split_node->free = 0;
        return split_node->start;
    }
    uint32_t start_addr, d = pow(2, starting_list->size -1), p_start, p_end;
    BuddyList* p_l = starting_list, *tmp = NULL;
    BuddyNode* p_n, *tmp_n, *tmp_n2;
    start_addr = split_node->start;
    bool delete_list = DeleteNode(split_node, starting_list);

    
    if (p_l->prev == NULL) {
        tmp = AddList();
        p_l->prev = tmp;
        tmp->next = p_l;
        tmp->prev = NULL;
    }
    else if (p_l->prev->size == p_l->size - 1) {
        tmp = AddList();
        p_l->prev->next = tmp;
        tmp->prev = p_l->prev;
        tmp->next = p_l;
        p_l->prev = tmp;
    }
    if (tmp != NULL) tmp->size = p_l->size - 1;

    p_l = p_l->prev;

    if (delete_list == false) {
        DeleteList(starting_list);
    }

    p_n = p_l->head;

    while (p_n != NULL) {
        p_start = p_n->start;
        p_end = p_n->next->start;
        if (p_n->next == NULL) p_end = start_addr + 1;
         
        if (p_n->start < start_addr && start_addr < p_end) {
            tmp_n = AddNode();
            tmp_n->id = curr_id++;
            tmp_n->free = true;
            tmp_n->buddy = curr_id;
            tmp_n->start = start_addr;
            tmp_n2 = AddNode();
            tmp_n2->id = curr_id;
            tmp_n2->buddy = curr_id - 1;
            curr_id++;
            tmp_n2->free = true;
            tmp_n2->start = start_addr + d;
            tmp_n->next = tmp_n2;
            tmp_n2->prev = tmp_n;
            tmp_n2->next = p_n->next;
            p_n->next = tmp_n;
            p_n = tmp_n;
            break;
        }
        p_n = p_n->next;
    }

    if (p_n == NULL) {
        tmp_n = AddNode();
        tmp_n->id = curr_id++;
        tmp_n->free = true;
        tmp_n->buddy = curr_id;
        tmp_n->start = start_addr;
        p_l->head = tmp_n;
        tmp_n2 = AddNode();
        tmp_n2->id = curr_id;
        tmp_n2->buddy = curr_id - 1;
        curr_id++;
        tmp_n2->free = true;
        tmp_n2->start = start_addr + d;
        tmp_n2->next = NULL;
        tmp_n2->prev = tmp_n;
        tmp_n->next = tmp_n2;
        p_n = tmp_n;
    }

    uint32_t ret = SplitList(p_l, bit_pos, p_n);
    if (p_n->free == 1) {
        DeleteNode(p_n, p_l);
    }
    return ret;
}


void* brk(BuddyList* list, uint32_t size) {
    BuddyList* p = list;
    BuddyNode* p_n;
    
    size = GetLargestBit(size);
    if (NumOfActiveBits(size) != 1) size++;        

    while (p->size <= size) {
        if (p->size == size) {
            p_n = p->head;
            while (p_n->free == 0) {
                if (p_n->next == NULL) {
                    break;
                }
                p_n = p_n->next;
            }
            if (p_n->free == 1) {
                p_n->free = 0;
                return (void*)p_n->start;
            }
        }
        p = p->next;
    }
    while (p != NULL) {
        p_n = p->head;
        while (p_n->free == 0) {
            if (p_n->next == NULL) {
                break;
            }
            p_n = p_n->next;
        }
        if (p_n->free == 0) {
            p = p->next;
            continue;
        }
        return (void*)SplitList(p, size, p_n);
    }
    return NULL;
}
