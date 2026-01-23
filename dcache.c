#include "dcache.h"

static dcache_entry_t* dcache_table[DCACHE_SIZE];

uint32_t hash_dentry(vfs_dentry_t* parent, const char* name) {
    uint32_t hash = (uint32_t)parent;
    

    hash = hash * GOLDEN_RATIO_32; 

    int c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c; 
    }

    return hash % DCACHE_SIZE;
}

void dCachePut(vfs_dentry_t* dentry) {
    vfs_dentry_t* parent = dentry->parent;
    uint32_t bucket = hash_dentry(parent, dentry->name);
    
    dcache_entry_t* entry = kmalloc(sizeof(dcache_entry_t));
    entry->dentry = dentry;
    
    entry->next = dcache_table[bucket];
    dcache_table[bucket] = entry;
}

void dCacheRemove(vfs_dentry_t* dentry) {
    vfs_dentry_t* parent = dentry->parent;
    char* name = dentry->name;


    uint32_t bucket = hash_dentry(parent, dentry->name);
    

    dcache_entry_t* p = dcache_table[bucket];
    if (p == NULL) kerror("Tried to remove a dentry which wasn't in hashmap");

    if (p->dentry->parent == parent && strcmp(p->dentry->name, name)) {
        kfree(dentry, sizeof(vfs_dentry_t));
        dcache_table[bucket] = dcache_table[bucket]->next;
    }

    if (p->next == NULL) {
        dcache_table[bucket] = NULL;
        return;
    }
    while (p->next != NULL) {
        if (p->next->dentry->parent == parent && strcmp(p->next->dentry->name, name)) {
            kfree(dentry, sizeof(vfs_dentry_t));
            p->next = p->next->next;
            return;
        }
        p = p->next;
    }
    kerror("Tried to remove a dentry which wasn't in hashmap");
}

vfs_dentry_t* dCacheLookup(vfs_dentry_t* parent, const char* name) {
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