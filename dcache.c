#include "dcache.h"

static dcache_entry_t* dcache_table[DCACHE_SIZE];

uint32_t hash_dentry(dentry_t* parent, const char* name) {
    uint32_t hash = (uint32_t)parent;
    

    hash = hash * GOLDEN_RATIO_32; 

    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c; 
    }

    return hash % DCACHE_SIZE;
}

void dCachePut(dentry_t* dentry) {
    bool sti = check_interrupts();
    CliHelper();
    dentry_t* parent = dentry->parent;
    char* name = dentry->name;
    uint32_t bucket = hash_dentry(parent, name);
    
    dcache_entry_t* entry = kmalloc(sizeof(dcache_entry_t));
    entry->dentry = dentry;
    
    entry->next = dcache_table[bucket];
    dcache_table[bucket] = entry;
    if (sti) StiHelper();
}

void dCacheRemove(dentry_t* dentry) {
    dentry_t* parent = dentry->parent;
    char* name = dentry->name;
    uint32_t bucket = hash_dentry(parent, name);

    dcache_entry_t* p = dcache_table[bucket];
    if (p == NULL) return;
    
    if (p->dentry == dentry) { 
        dcache_entry_t* to_free = p;
        dcache_table[bucket] = p->next;
        kfree(to_free, sizeof(dcache_entry_t)); 
        return;
    }

    while (p->next != NULL) {
        if (p->next->dentry == dentry) {
            dcache_entry_t* to_free = p->next;
            p->next = p->next->next;
            kfree(to_free, sizeof(dcache_entry_t)); 
            return;
        }
        p = p->next;
    }
}

dentry_t* dCacheLookup(dentry_t* parent, const char* name) {
    uint32_t bucket = hash_dentry(parent, name);
    dcache_entry_t* current = dcache_table[bucket];

    while (current) {
        if (current->dentry->parent == parent && strcmp(current->dentry->name, name) == 0) {
            return current->dentry;
        }
        current = current->next;
    }
    return NULL;
}