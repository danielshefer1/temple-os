#include "blocks_buffer.h"

static buffer_node_t* hash_table[BUFFER_CACHE_SIZE];
static buffer_cache_t buffer_cache = {
    .hash_table = hash_table,
    .size = 0,
    .capacity = BUFFER_CACHE_CAP,
    .hash_table_length = BUFFER_CACHE_SIZE,
    .lru_head = NULL,
    .lru_tail = NULL
};

uint32_t EXT2SectorsInBlock(superblock_t* sb) {
    return sb->block_size / sb->bdev->sector_size;
}

uint64_t EXT2BlockToLba(superblock_t* sb, uint32_t block_idx) {
    uint32_t sectors_per_block = sb->block_size / sb->bdev->sector_size;

    return sb->start_lba + (block_idx * sectors_per_block);
}

int64_t EXT2ReadBlocks(superblock_t* sb, uint32_t block_idx, uint32_t count, void* buf) {
    if (sb == NULL || buf == NULL || count == 0) return 1;

    uint64_t sectors_count = count * EXT2SectorsInBlock(sb);
    sb->bdev->read(sb->bdev, EXT2BlockToLba(sb, block_idx), count * EXT2SectorsInBlock(sb), (void*)KERNEL_VIRT_TO_PHYS(buf));
    return 0;

}

int64_t EXT2WriteBlocks(superblock_t* sb, uint32_t block_idx, uint32_t count, void* buf) {
    if (sb == NULL || buf == NULL || count == 0) return 1;

    uint64_t sectors_count = count * EXT2SectorsInBlock(sb);
    sb->bdev->write(sb->bdev, EXT2BlockToLba(sb, block_idx), count * EXT2SectorsInBlock(sb), (void*)KERNEL_VIRT_TO_PHYS(buf));
    return 0;

}

void DeleteNodeFromLRU(buffer_node_t* node) {
    if (node == NULL) return;

    if (node->prev != NULL) {
        node->prev->next = node->next;

    } else {
        buffer_cache.lru_tail = node->next;
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;

    } else {
        buffer_cache.lru_head = node->prev;
    }
}

void DeleteNodeFromHash(buffer_node_t* node) {
    if (node == NULL) return;

    uint64_t hash_idx = node->block_number % BUFFER_CACHE_SIZE;
    buffer_node_t* current = buffer_cache.hash_table[hash_idx], *prev = NULL;

    while (current != NULL) {
        if (current == node) {
            if (prev != NULL) {
                prev->hash_next = current->hash_next;
            }
            else {
                buffer_cache.hash_table[hash_idx] = current->hash_next;
            }
            return;
        }
        prev = current;
        current = current->hash_next;
    }
}

void AddNodeToLRUHead(buffer_node_t* node) {
    if (node == NULL) return;
    if (buffer_cache.lru_head == node) return;

    node->prev = buffer_cache.lru_head;
    node->next = NULL;

    if (buffer_cache.lru_head != NULL) {
        buffer_cache.lru_head->next = node;
    }
    buffer_cache.lru_head = node;

    if (buffer_cache.lru_tail == NULL) {
        buffer_cache.lru_tail = node;
    }
}

void SwitchNodeToLRUHead(buffer_node_t* node) {
    if (node == NULL || buffer_cache.lru_head == node) return;

    DeleteNodeFromLRU(node);
    AddNodeToLRUHead(node);
}

void* bread(superblock_t* sb, uint32_t block_number) {
    if (sb == NULL) return NULL;


    uint64_t hash_idx = block_number % BUFFER_CACHE_SIZE;
    mutex_lock(&buffer_cache.lock);
    buffer_node_t* node = buffer_cache.hash_table[hash_idx], *avi_node = NULL;

    while (node != NULL) {
        if (!node->is_valid) {
            avi_node = node;
        }
        if (node->block_number == block_number && node->is_valid) {
            void* data = node->data;
            node->ref_count++;

            SwitchNodeToLRUHead(node);

            mutex_unlock(&buffer_cache.lock);
            return data;
        }
        node = node->hash_next;
    }
    void* ret = (void*) AddKernelPages(sb->pages_in_block);
    if (ret == NULL) {
        mutex_unlock(&buffer_cache.lock);
        kerror("Failed to allocate memory for block buffer!");
        return NULL;
    }

    if (avi_node == NULL) {
        if (buffer_cache.size == buffer_cache.capacity) {
            buffer_node_t* del_node = buffer_cache.lru_tail;
            while (del_node != NULL && del_node->ref_count > 0) {
                del_node = del_node->next;
            }
            if (del_node == NULL) {kerror("Buffer cache is full and all nodes are in use!"); return NULL;}

            if (del_node->is_dirty) {
                EXT2WriteBlocks(sb, del_node->block_number, 1, del_node->data);
            }
            RemoveKernelPages((uint64_t)del_node->data, sb->pages_in_block);

            DeleteNodeFromLRU(del_node);
            DeleteNodeFromHash(del_node);

            kfree(del_node, sizeof(buffer_node_t));
        }
        else {
            buffer_cache.size++;
        }

        avi_node = (buffer_node_t*) kmalloc(sizeof(buffer_node_t));
        avi_node->hash_next = buffer_cache.hash_table[hash_idx];
        buffer_cache.hash_table[hash_idx] = avi_node;

        AddNodeToLRUHead(avi_node);
    }

    avi_node->is_valid = true;
    avi_node->block_number = block_number;
    avi_node->data = ret;
    avi_node->ref_count = 1;

    SwitchNodeToLRUHead(avi_node);
    mutex_unlock(&buffer_cache.lock);

    EXT2ReadBlocks(sb, block_number, 1, ret);
    return ret;
}

void brelse(superblock_t* sb, uint32_t block_number) {
    if (sb == NULL) return;

    uint64_t hash_idx = block_number % BUFFER_CACHE_SIZE;
    mutex_lock(&buffer_cache.lock);
    buffer_node_t* node = buffer_cache.hash_table[hash_idx];

    while (node != NULL) {
        if (node->block_number == block_number && node->is_valid) {
            if (node->ref_count > 0) {
                node->ref_count--;
            }
            mutex_unlock(&buffer_cache.lock);
            return;
        }
        node = node->hash_next;
    }
    mutex_unlock(&buffer_cache.lock);
}

void bwrite(superblock_t* sb, uint32_t block_number) {
    if (sb == NULL) return;

    uint64_t hash_idx = block_number % BUFFER_CACHE_SIZE;
    mutex_lock(&buffer_cache.lock);
    buffer_node_t* node = buffer_cache.hash_table[hash_idx];

    while (node != NULL) {
        if (node->block_number == block_number && node->is_valid) {
            node->is_dirty = true;
            SwitchNodeToLRUHead(node);

            mutex_unlock(&buffer_cache.lock);
            return;
        }
        node = node->hash_next;
    }
    mutex_unlock(&buffer_cache.lock);
}

void bflush(superblock_t* sb, uint32_t block_number) {
    if (sb == NULL) return;
    uint64_t hash_idx = block_number % BUFFER_CACHE_SIZE;

    mutex_lock(&buffer_cache.lock);

    buffer_node_t* node = buffer_cache.hash_table[hash_idx];
    while (node != NULL) {
        if (node->block_number == block_number && node->is_valid) {
            if (node->is_dirty) {
                EXT2WriteBlocks(sb, node->block_number, 1, node->data);
                node->is_dirty = false;
            }
            SwitchNodeToLRUHead(node);
            mutex_unlock(&buffer_cache.lock);
            return;
        }
        node = node->hash_next;
    }
    mutex_unlock(&buffer_cache.lock);
}

void bflush_all(superblock_t* sb) {
    if (sb == NULL) return;

    mutex_lock(&buffer_cache.lock);
    
    buffer_node_t* node = buffer_cache.lru_head;
    while (node != NULL) {
        if (node->is_dirty) {
            EXT2WriteBlocks(sb, node->block_number, 1, node->data);
            node->is_dirty = false;
        }
        node = node->prev;
    }

    mutex_unlock(&buffer_cache.lock);
}

void binvalidate(superblock_t* sb, uint32_t block_number) {
    if (sb == NULL) return;
    uint64_t hash_idx = block_number % BUFFER_CACHE_SIZE;

    mutex_lock(&buffer_cache.lock);

    buffer_node_t* node = buffer_cache.hash_table[hash_idx];
    while (node != NULL) {
        if (node->block_number != block_number || !node->is_valid) {
            node = node->hash_next;
            continue;
        }

        node->is_dirty = false;
        node->is_valid = false;
        DeleteNodeFromHash(node);
        DeleteNodeFromLRU(node);
        RemoveKernelPages((uint64_t)node->data, sb->pages_in_block);
        kfree(node, sizeof(buffer_node_t));
        buffer_cache.size--;

        mutex_unlock(&buffer_cache.lock);
        return;
    }
    mutex_unlock(&buffer_cache.lock);
}

void bclean(superblock_t* sb, uint32_t block_number) {
    if (sb == NULL) return;
    uint64_t hash_idx = block_number % BUFFER_CACHE_SIZE;

    mutex_lock(&buffer_cache.lock);

    buffer_node_t* node = buffer_cache.hash_table[hash_idx];
    while (node != NULL) {
        if (node->block_number != block_number || node->is_valid) {
            node = node->hash_next;
            continue;
        }

        if (node->is_dirty) {
            EXT2WriteBlocks(sb, node->block_number, 1, node->data);
            node->is_dirty = false;
        }   
        DeleteNodeFromHash(node);
        DeleteNodeFromLRU(node);
        kfree(node, sizeof(buffer_node_t));

        mutex_unlock(&buffer_cache.lock);
        return;
    }
    mutex_unlock(&buffer_cache.lock);
}

void bclean_all(superblock_t* sb) {
    if (sb == NULL) return;

    mutex_lock(&buffer_cache.lock);

    buffer_node_t* p = buffer_cache.lru_head, *p1;
    while (p->prev != NULL) {
        if (p->is_dirty) {
            EXT2WriteBlocks(sb, p->block_number, 1, p->data);
        }
        RemoveKernelPages((uint64_t)p->data, sb->block_size / PAGE_SIZE);

        p1 = p->prev;
        kfree(p, sizeof(buffer_node_t));
        p = p1;
    }
    if (p->is_dirty) {
        EXT2WriteBlocks(sb, p->block_number, 1, p->data);
    }
    RemoveKernelPages((uint64_t)p->data, sb->block_size / PAGE_SIZE);
    kfree(p, sizeof(buffer_node_t));

    memset(buffer_cache.hash_table, 0, buffer_cache.hash_table_length * sizeof(void*));
    buffer_cache.lru_head = NULL;
    buffer_cache.lru_tail = NULL;
    buffer_cache.size = 0;

    mutex_unlock(&buffer_cache.lock);
}