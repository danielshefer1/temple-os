#include "dcache.h"

static dcache_entry_t* dcache_table[DCACHE_SIZE];
static mutex_t dcache_lock;

uint64_t hash_dentry(dentry_t* parent, const char* name) {
    uint64_t hash = (uint64_t)parent;
    

    hash = hash * GOLDEN_RATIO_32; 

    uint64_t c;
    while ((c = *name++)) {
        hash = ((hash << 5) + hash) + c; 
    }

    return hash % DCACHE_SIZE;
}

void dCachePut(dentry_t* dentry) {
    dentry_t* parent = dentry->parent;
    char* name = dentry->name;
    uint64_t bucket = hash_dentry(parent, name);

    dcache_entry_t* entry = kmalloc(sizeof(dcache_entry_t));
    entry->dentry = dentry;

    mutex_lock(&dcache_lock);
    entry->next = dcache_table[bucket];
    dcache_table[bucket] = entry;
    mutex_unlock(&dcache_lock);
}

void dCacheRemove(dentry_t* dentry) {
    dentry_t* parent = dentry->parent;
    char* name = dentry->name;
    uint64_t bucket = hash_dentry(parent, name);

    mutex_lock(&dcache_lock);
    dcache_entry_t* p = dcache_table[bucket];
    if (p == NULL) {
        mutex_unlock(&dcache_lock);
        return;
    }

    if (p->dentry == dentry) {
        dcache_entry_t* to_free = p;
        dcache_table[bucket] = p->next;
        mutex_unlock(&dcache_lock);
        kfree(to_free, sizeof(dcache_entry_t));
        return;
    }

    while (p->next != NULL) {
        if (p->next->dentry == dentry) {
            dcache_entry_t* to_free = p->next;
            p->next = p->next->next;
            mutex_unlock(&dcache_lock);
            kfree(to_free, sizeof(dcache_entry_t));
            return;
        }
        p = p->next;
    }
    mutex_unlock(&dcache_lock);
}

dentry_t* dCacheLookup(dentry_t* parent, const char* name) {
    uint64_t bucket = hash_dentry(parent, name);

    mutex_lock(&dcache_lock);
    dcache_entry_t* current = dcache_table[bucket];

    while (current) {
        if (current->dentry->parent == parent && strcmp(current->dentry->name, name) == 0) {
            dentry_t* d = current->dentry;
            mutex_unlock(&dcache_lock);
            return d;
        }
        current = current->next;
    }
    mutex_unlock(&dcache_lock);
    return NULL;
}