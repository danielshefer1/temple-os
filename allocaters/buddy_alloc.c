#include "buddy_alloc.h"

static buddy_bin_t user_bins[MAX_ORDER];
static buddy_bin_t kernel_bins[MAX_ORDER];
static uint64_t user_lowest_vaild = UINT64_MAX;
static uint64_t kernel_lowest_vaild = UINT64_MAX;

// Shadow side-table for the kernel buddy pool. See header.
// Indexed by (pfn - kshadow_base_pfn). One byte per page frame.
// Bits 0..5 = order; bit 6 = in-free-list; bit 7 = reserved.
// 0 means "not a tracked block head". Step-2: maintained through every
// allocator state change (split, merge, free). Step-3 will replace the
// linked-list lookups with reads of this table.
#define KSHADOW_ORDER_MASK 0x3F
#define KSHADOW_FREE_FLAG  0x40
static uint8_t* kshadow = NULL;
static uint64_t kshadow_base_pfn = 0;
static uint64_t kshadow_pfn_count = 0;

static inline bool kshadow_in_range(uint64_t addr) {
    if (kshadow == NULL) return false;
    uint64_t pfn = addr >> PAGE_SIZE_LOG2;
    return pfn >= kshadow_base_pfn && pfn < kshadow_base_pfn + kshadow_pfn_count;
}

static inline void kshadow_set_free(uint64_t addr, uint64_t order) {
    if (!kshadow_in_range(addr)) return;
    kshadow[(addr >> PAGE_SIZE_LOG2) - kshadow_base_pfn] =
        (uint8_t)((order & KSHADOW_ORDER_MASK) | KSHADOW_FREE_FLAG);
}

static inline void kshadow_set_used(uint64_t addr, uint64_t order) {
    if (!kshadow_in_range(addr)) return;
    kshadow[(addr >> PAGE_SIZE_LOG2) - kshadow_base_pfn] =
        (uint8_t)(order & KSHADOW_ORDER_MASK);
}

static inline void kshadow_clear(uint64_t addr) {
    if (!kshadow_in_range(addr)) return;
    kshadow[(addr >> PAGE_SIZE_LOG2) - kshadow_base_pfn] = 0;
}

// Inline free list for the kernel buddy pool. Each free page stores a single
// pointer (the next free page at the same order, as a physical address) at
// its first 8 bytes, accessed via KERNEL_VIRTUAL. Heads live in this array.
// LIFO push/pop — most-recently-freed page is hottest in cache.
//
// kernel_nonempty_mask: bit N set iff kernel_inline_heads[N] != NULL.
// Lets the order-search in kernel_request_buddy be a single TZCNT instead
// of a MAX_ORDER linear walk. Maintained on every push/pop/remove.
//
// This replaces the kernel's buddy_node_t linked list with no per-block
// metadata allocation. The user pool still uses the struct-based lists.
static void* kernel_inline_heads[MAX_ORDER];
static uint64_t kernel_nonempty_mask;

static inline void** inline_slot(void* phys) {
    return (void**)((uint64_t)phys + KERNEL_VIRTUAL);
}

static inline void inline_push(uint64_t order, void* phys) {
    *inline_slot(phys) = kernel_inline_heads[order];
    kernel_inline_heads[order] = phys;
    kernel_nonempty_mask |= (1ULL << order);
}

static inline void* inline_pop(uint64_t order) {
    void* head = kernel_inline_heads[order];
    if (head != NULL) {
        kernel_inline_heads[order] = *inline_slot(head);
        if (kernel_inline_heads[order] == NULL) {
            kernel_nonempty_mask &= ~(1ULL << order);
        }
    }
    return head;
}

static bool inline_remove(uint64_t order, void* phys) {
    void** prev_link = &kernel_inline_heads[order];
    void* cur = *prev_link;
    while (cur != NULL) {
        if (cur == phys) {
            *prev_link = *inline_slot(cur);
            if (kernel_inline_heads[order] == NULL) {
                kernel_nonempty_mask &= ~(1ULL << order);
            }
            return true;
        }
        prev_link = inline_slot(cur);
        cur = *prev_link;
    }
    return false;
}

// Per-pool spinlocks. Held across RequestBuddy/FreeBuddy. The internal
// helpers (SplitNode, MergeBuddy, MoveBuddyNode, CreateBuddyNode) call
// kmalloc/kfree, which take per-cache slab locks — order is buddy > slab.
// Same-CPU recursion (kmalloc -> AddSlabW -> AddKernelPages -> RequestBuddy)
// would deadlock; relies on the buddy_node_t slab cache staying warm.
static spinlock_t kernel_buddy_lock;
static spinlock_t user_buddy_lock;

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
        if (user) {
            // User pool: keep struct-based list.
            buddy_node_t* node = CreateBuddyNode((void*)start, bit);
            InsertSortedBuddyNode(&bins[bit], node, true);
        } else {
            // Kernel pool: inline next-pointer in the page itself + shadow.
            inline_push(bit, (void*)start);
            kshadow_set_free(start, bit);
        }

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
        kshadow_set_used((uint64_t)ret, target_order);
        return ret;
    }
    uint64_t addr = (uint64_t)node->address, curr_order = node->order;
    RemoveBuddyNode(&bins[node->order], node->address, true);

    buddy_node_t* target_buddy1 = CreateBuddyNode((void*)addr, target_order);
    target_buddy1->free = false;
    InsertSortedBuddyNode(&bins[target_order], target_buddy1, false);
    kshadow_set_used(addr, target_order);

    while (curr_order > target_order) {
        void* buddy_address = GetBuddyAddress((void*)addr, curr_order - 1);
        buddy_node_t* buddy2 = CreateBuddyNode(buddy_address, curr_order - 1);
        InsertSortedBuddyNode(&bins[curr_order - 1], buddy2, true);
        kshadow_set_free((uint64_t)buddy_address, curr_order - 1);
        curr_order--;
    }
    return target_buddy1->address;
}

bool MergeBuddy(void* address, uint64_t order, buddy_bin_t* bins) {
    void* buddy_address = GetBuddyAddress(address, order);
    bool found = false;

    if (kshadow_in_range((uint64_t)buddy_address)) {
        // O(1) presence check via shadow. The list is removed by address below
        // (RemoveBuddyNodeComplete), so we don't need the node pointer.
        uint8_t s = kshadow[((uint64_t)buddy_address >> PAGE_SIZE_LOG2) - kshadow_base_pfn];
        uint8_t want = (uint8_t)((order & KSHADOW_ORDER_MASK) | KSHADOW_FREE_FLAG);
        found = (s == want);
    } else {
        // User pool / unshadowed address: walk the free list as before.
        buddy_node_t* buddy_node = bins[order].head_free;
        while (buddy_node != NULL) {
            if (buddy_node->address == buddy_address) {
                found = true;
                break;
            }
            buddy_node = buddy_node->next;
        }
    }

    if (found) {
        RemoveBuddyNodeComplete(&bins[order], address);
        RemoveBuddyNodeComplete(&bins[order], buddy_address);

        void* merged_address = (address < buddy_address) ? address : buddy_address;
        void* other_address  = (address < buddy_address) ? buddy_address : address;
        buddy_node_t* merged_node = CreateBuddyNode(merged_address, order + 1);
        merged_node->free = false;
        InsertSortedBuddyNode(&bins[order + 1], merged_node, false);

        // Shadow: only one head survives at the higher order; the other PFN
        // is no longer a block head. Mark merged eagerly as FREE — if the
        // recursive call merges further it will overwrite at order+2; if not,
        // MoveBuddyNode below flips the bin to match.
        kshadow_clear((uint64_t)other_address);
        kshadow_set_free((uint64_t)merged_address, order + 1);

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

// Kernel-only fast path. Uses the inline free list and the shadow exclusively;
// never calls kmalloc/kfree, so the historic recursion chain (RequestBuddy →
// CreateBuddyNode → kmalloc(buddy_node_t) → AddSlabW → AddKernelPages →
// RequestBuddy) cannot form. Caller must hold no locks that this function
// would re-take. Returns physical addresses, like RequestBuddy.
static void* kernel_request_buddy(uint64_t size) {
    uint64_t order = BiggestBit(size);
    if (!IsPowerOfTwo(size)) order++;
    if (order < PAGE_SIZE_LOG2) order = PAGE_SIZE_LOG2;
    if (order >= MAX_ORDER) return NULL;

    bool org_int_state = check_interrupts();
    CliHelper();
    spin_lock(&kernel_buddy_lock);

    // O(1) order search: mask off orders below target, find the lowest
    // remaining set bit. TZCNT on x86 is single-cycle.
    uint64_t hi_mask = kernel_nonempty_mask & ~((1ULL << order) - 1);
    if (hi_mask == 0) {
        spin_unlock(&kernel_buddy_lock);
        if (org_int_state) StiHelper();
        return NULL;
    }
    uint64_t curr = (uint64_t)__builtin_ctzll(hi_mask);

    void* block = inline_pop(curr);
    while (curr > order) {
        curr--;
        void* buddy = (void*)((uint64_t)block ^ (1ULL << curr));
        inline_push(curr, buddy);
        kshadow_set_free((uint64_t)buddy, curr);
    }
    kshadow_set_used((uint64_t)block, order);

    spin_unlock(&kernel_buddy_lock);
    if (org_int_state) StiHelper();
    return block;
}

static void kernel_free_buddy(void* address) {
    if (!kshadow_in_range((uint64_t)address)) return;

    bool org_int_state = check_interrupts();
    CliHelper();
    spin_lock(&kernel_buddy_lock);

    uint64_t pfn_idx = ((uint64_t)address >> PAGE_SIZE_LOG2) - kshadow_base_pfn;
    uint8_t s = kshadow[pfn_idx];
    if (s == 0 || (s & KSHADOW_FREE_FLAG)) {
        // Not allocated, or already free — silently bail (matches old FreeBuddy).
        spin_unlock(&kernel_buddy_lock);
        if (org_int_state) StiHelper();
        return;
    }
    uint64_t order = s & KSHADOW_ORDER_MASK;

    // Coalesce upward as long as the buddy is free at the same order.
    while (order + 1 < MAX_ORDER) {
        uint64_t buddy_addr = (uint64_t)address ^ (1ULL << order);
        if (!kshadow_in_range(buddy_addr)) break;
        uint8_t bs = kshadow[(buddy_addr >> PAGE_SIZE_LOG2) - kshadow_base_pfn];
        uint8_t want = (uint8_t)((order & KSHADOW_ORDER_MASK) | KSHADOW_FREE_FLAG);
        if (bs != want) break;

        if (!inline_remove(order, (void*)buddy_addr)) {
            // Shadow says free but list disagrees — invariant break, stop merging.
            break;
        }
        uint64_t lower  = (uint64_t)address < buddy_addr ? (uint64_t)address : buddy_addr;
        uint64_t higher = (uint64_t)address < buddy_addr ? buddy_addr : (uint64_t)address;
        kshadow_clear(higher);
        address = (void*)lower;
        order++;
    }

    inline_push(order, address);
    kshadow_set_free((uint64_t)address, order);

    spin_unlock(&kernel_buddy_lock);
    if (org_int_state) StiHelper();
}

void FreeBuddy(void* address, bool user) {
    if (!user) { kernel_free_buddy(address); return; }

    buddy_bin_t* bins = user_bins;
    uint64_t* lowest_valid_p = &user_lowest_vaild;
    spinlock_t* lock = &user_buddy_lock;

    bool org_int_state = check_interrupts();
    CliHelper();
    spin_lock(lock);

    uint64_t order = 0, page_count;
    buddy_node_t* node = FindBuddyNode(&bins[order], address);
    while (node == NULL && order < MAX_ORDER) {
        order++;
        node = FindBuddyNode(&bins[order], address);
    }
    if (node == NULL) {
        spin_unlock(lock);
        if (org_int_state) StiHelper();
        return;
    }
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

    *lowest_valid_p = FindLowest(bins);

    spin_unlock(lock);
    if (org_int_state) StiHelper();
}

void* RequestBuddy(uint64_t size, bool user) {
    if (!user) return kernel_request_buddy(size);

    buddy_bin_t* bins = user_bins;
    uint64_t* lowest_vaild_p = &user_lowest_vaild;
    spinlock_t* lock = &user_buddy_lock;

    uint64_t order = BiggestBit(size), tmp;
    if (!IsPowerOfTwo(size)) order++;

    if (order < PAGE_SIZE_LOG2 + 1) {
        order = PAGE_SIZE_LOG2;
    }

    if (order >= MAX_ORDER) {
        return NULL;
    }

    bool org_int_state = check_interrupts();
    CliHelper();
    spin_lock(lock);

    if (order < *lowest_vaild_p) {
        tmp = order;
        order = *lowest_vaild_p;
        *lowest_vaild_p = tmp;
    }

    for (uint64_t current_order = order; current_order < MAX_ORDER; current_order++) {
        if (bins[current_order].head_free != NULL) {
            void* ret = SplitNode(bins[current_order].head_free, order, bins);
            if (ret != NULL) {
                spin_unlock(lock);
                if (org_int_state) StiHelper();
                return ret;
            }
        }
    }
    spin_unlock(lock);
    if (org_int_state) StiHelper();
    return NULL;
}

void* GetBuddyAddress(void* address, uint64_t order) {
    // A block of order N spans 2^N bytes; its buddy is at addr ^ 2^N.
    // Previously this used (order - 1), which gave the buddy of a 2^(N-1)
    // block — meaning consecutive same-order allocations overlapped by half
    // their size. Surfaced as kernel-stack clobbering on SMP under load.
    uint64_t addr = (uint64_t)address;
    uint64_t buddy_addr = addr ^ (1ULL << order);
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

uint64_t buddy_kernel_total_pages(void) {
    // Every PFN tracked by the shadow is a page in the kernel pool.
    return kshadow_pfn_count;
}

uint64_t buddy_kernel_free_pages(void) {
    if (kshadow == NULL) return 0;
    bool sti = check_interrupts();
    CliHelper();
    spin_lock(&kernel_buddy_lock);
    uint64_t pages = 0;
    for (uint64_t order = 0; order < MAX_ORDER; order++) {
        for (void* p = kernel_inline_heads[order]; p != NULL; p = *inline_slot(p)) {
            pages += (1ULL << order) >> PAGE_SIZE_LOG2;
        }
    }
    spin_unlock(&kernel_buddy_lock);
    if (sti) StiHelper();
    return pages;
}

uint64_t buddy_user_total_pages(void) {
    bool sti = check_interrupts();
    CliHelper();
    spin_lock(&user_buddy_lock);
    uint64_t pages = 0;
    for (uint64_t order = 0; order < MAX_ORDER; order++) {
        for (buddy_node_t* n = user_bins[order].head_free; n != NULL; n = n->next) {
            pages += (1ULL << order) >> PAGE_SIZE_LOG2;
        }
        for (buddy_node_t* n = user_bins[order].head_used; n != NULL; n = n->next) {
            pages += (1ULL << order) >> PAGE_SIZE_LOG2;
        }
    }
    spin_unlock(&user_buddy_lock);
    if (sti) StiHelper();
    return pages;
}

uint64_t buddy_user_free_pages(void) {
    bool sti = check_interrupts();
    CliHelper();
    spin_lock(&user_buddy_lock);
    uint64_t pages = 0;
    for (uint64_t order = 0; order < MAX_ORDER; order++) {
        for (buddy_node_t* n = user_bins[order].head_free; n != NULL; n = n->next) {
            pages += (1ULL << order) >> PAGE_SIZE_LOG2;
        }
    }
    spin_unlock(&user_buddy_lock);
    if (sti) StiHelper();
    return pages;
}

void InitKernelBuddyShadow(uint64_t pool_start_phys, uint64_t pool_end_phys) {
    if (pool_end_phys <= pool_start_phys) return;
    kshadow_base_pfn  = pool_start_phys >> PAGE_SIZE_LOG2;
    kshadow_pfn_count = (pool_end_phys - pool_start_phys) >> PAGE_SIZE_LOG2;

    uint64_t bytes = kshadow_pfn_count;  // 1 byte per page frame
    uint64_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    // AddKernelPagesPrimitive returns kernel-virt; the table is read/written
    // by name only, so a kernel-virt pointer is what we want.
    kshadow = (uint8_t*) AddKernelPagesPrimitive(pages);
    memset(kshadow, 0, bytes);
}

bool VerifyKernelBuddyShadow(void) {
    if (kshadow == NULL) {
        kprintf("buddy shadow: not initialised\n");
        return false;
    }

    uint64_t free_count = 0, used_count = 0, shadow_count = 0, mismatches = 0;

    bool sti = check_interrupts();
    CliHelper();

    // Walk the inline free list per order; each block must have a matching
    // shadow byte with the FREE flag set.
    for (uint64_t order = 0; order < MAX_ORDER; order++) {
        for (void* p = kernel_inline_heads[order]; p != NULL; p = *inline_slot(p)) {
            free_count++;
            uint64_t pfn = (uint64_t)p >> PAGE_SIZE_LOG2;
            if (pfn < kshadow_base_pfn || pfn >= kshadow_base_pfn + kshadow_pfn_count) {
                mismatches++;
                continue;
            }
            uint8_t s = kshadow[pfn - kshadow_base_pfn];
            uint8_t want = (uint8_t)((order & KSHADOW_ORDER_MASK) | KSHADOW_FREE_FLAG);
            if (s != want) mismatches++;
        }
    }

    // Count the shadow: free + used bytes. Used bytes are the non-FREE-flagged
    // entries — they have no list to walk against, so we just tally them.
    for (uint64_t i = 0; i < kshadow_pfn_count; i++) {
        uint8_t s = kshadow[i];
        if (s == 0) continue;
        shadow_count++;
        if (!(s & KSHADOW_FREE_FLAG)) used_count++;
    }

    if (sti) StiHelper();

    kprintf("buddy shadow: free=%d used=%d shadow=%d mismatches=%d\n",
            free_count, used_count, shadow_count, mismatches);
    return (free_count + used_count == shadow_count) && (mismatches == 0);
}