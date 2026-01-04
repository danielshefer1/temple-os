#include "buddy_alloc.h"

BuddyList* list;



void InitBuddyAlloc(uint32_t start, uint32_t size) {
    if (size == 0) return;
    list = (BuddyList*) kmalloc(sizeof(BuddyList));

    while (size > 0) {
        list->size = BiggestBit(size);
        size -= 1 << list->size;
        list->head = (BuddyNode*) kmalloc(sizeof(BuddyNode));
        list->head->free = true;
        list->head->start = (void*) start;
        start += 1 << list->size;
        list->head->next = NULL;
        list->head->prev = NULL;
        list->head->list = list;
        list->next = NULL;
        list->prev = (BuddyList*) kmalloc(sizeof(BuddyList));
        list->prev->next = list;
        list = list->prev;       
    }
    list = list->next;
}

void CreatePrevList(BuddyList* list) {
    if (list->prev == NULL || list->prev->size != list->size) {
        BuddyList* tmp = (BuddyList*) kmalloc(sizeof(BuddyList));
        tmp->size = list->size - 1;
        tmp->next = list;
        tmp->prev = list->prev;
        list->prev = tmp;
    }
}

void* SplitNode(BuddyNode* node, uint32_t size) {
    list = node->list;

    if (list->size < size) {
        return NULL;
    }
    if (list->size == size) {
        node->free = false;
        return node->start;
    }

    if (list->prev == NULL || list->prev->size != list->size - 1) {
        CreatePrevList(list);
    }

    BuddyNode* curr_node;

    list = list->prev;   
    for (int i = 0; i < 2; i++) {
        BuddyNode* new_node = (BuddyNode*) kmalloc(sizeof(BuddyNode));
        new_node->free = true;
        new_node->start = (void*) (((uint32_t) node->start) + i * (1 << (list->size - 1)));
        new_node->next = NULL;
        new_node->prev = NULL;
        new_node->list = list;
        list->head = InsertNodeSorted(new_node, list->head);
        if (i == 0) curr_node = new_node;
    }

    void* ret = SplitNode(curr_node, size);
    if (curr_node->free == true) {
        DeleteNode(curr_node);
    }
    return ret;
}

void* RequestBlock(uint32_t size) {
    if (IsPowerOfTwo(size)) size = BiggestBit(size);
    else size = BiggestBit(size) + 1;
    
    GoToBack();
    while (list->size < size) {
        list = list->next;
    }
    BuddyList* p = list;
    while (p != NULL) {
        BuddyNode* node = SearchFreeNode(p);
        if (node == NULL) {
            p = p->next;
            continue;
        }
        void* ret = SplitNode(node, size);
        DeleteGarbage();
        return ret;
    }
    return NULL;
}

BuddyNode* SearchFreeNode(BuddyList* list) {
    BuddyNode* p = list->head;
    while (p != NULL) {
        if (p->free == true) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

void DeleteNode(BuddyNode* node) {
    if (node == NULL) return;

    if (node->next == NULL && node->prev == NULL) {
        node->list->head = NULL;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }

    kfree(node, sizeof(BuddyNode));
}

BuddyNode* InsertNodeSorted(BuddyNode* node, BuddyNode* head) {
    if (head == NULL) {
        return node;
    }
    BuddyNode* p = head;

    while (p != NULL) {
        if (p->start > node->start) {
            break;
        }
        p = p->next;
    }

    node->next = p;
    node->prev = p->prev;
    if (p->prev != NULL) {
        p->prev->next = node;
    }
    p->prev = node;

    return head;
}

void GoToBack() {
    while (list->prev != NULL) {
        list = list->prev;
    }
}

void DeleteGarbage() {
    GoToBack();
    BuddyList* tmp;

    while (list != NULL) {
        if (list->head == NULL) {
            if (list->prev != NULL) {
                list->prev->next = list->next;
            }
            if (list->next != NULL) {
                list->next->prev = list->prev;
            }
            tmp = list;
            list = list->next;
            kfree(tmp, sizeof(BuddyList));
            list = list->prev;
        }
        list = list->next;
    }
}

void PrintNode(BuddyNode* node) {
    while (node != NULL) {
        kprintf("Start: %p\t", node->start);
        node = node->next;
    }
    kprintf("\n");
}

void PrintList() {
    GoToBack();
    while (list != NULL) {
        kprintf("Size: %d\n", list->size);
        PrintNode(list->head);
        list = list->next;
    }
    GoToBack();
}
