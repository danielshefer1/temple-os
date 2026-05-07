#include "ram_block.h"
#include "devfs.h"
#include "string.h"
#include "vfs_types.h"
#include "vfs_defs.h"

#define RAM_BLOCK_SIZE  4096   // one page — fits the slab allocator's largest cache

// Static backing buffer. Avoids kmalloc — the slab allocator's biggest
// cache is PAGE_SIZE, and we don't want to wire a buddy-allocator path
// just to test devfs.
static uint8_t ram_buffer[RAM_BLOCK_SIZE];

static int64_t ram_read(file_t* f, void* buf, uint64_t size) {
    if (f->position >= RAM_BLOCK_SIZE) return 0;
    uint64_t avail = RAM_BLOCK_SIZE - f->position;
    if (size > avail) size = avail;
    memcpy(buf, ram_buffer + f->position, size);
    f->position += size;
    return (int64_t)size;
}

static int64_t ram_write(file_t* f, const void* buf, uint64_t size) {
    if (f->position >= RAM_BLOCK_SIZE) return -ENOSPC;
    uint64_t avail = RAM_BLOCK_SIZE - f->position;
    if (size > avail) size = avail;
    memcpy(ram_buffer + f->position, buf, size);
    f->position += size;
    return (int64_t)size;
}

static int64_t ram_seek(file_t* f, int64_t offset, int64_t whence) {
    int64_t base;
    switch (whence) {
        case SEEK_SET: base = 0;                       break;
        case SEEK_CUR: base = (int64_t)f->position;    break;
        case SEEK_END: base = RAM_BLOCK_SIZE;          break;
        default:       return -EINVAL;
    }
    int64_t np = base + offset;
    if (np < 0) return -EINVAL;
    f->position = (uint64_t)np;
    return np;
}

static file_ops_t ram_fops = {
    .read  = ram_read,
    .write = ram_write,
    .seek  = ram_seek,
};

void ram_block_init(void) {
    memset(ram_buffer, 0, RAM_BLOCK_SIZE);
    devfs_register_block(1, 0, &ram_fops, ram_buffer);
}
