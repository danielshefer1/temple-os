#include "mem_devs.h"
#include "devfs.h"
#include "string.h"
#include "vfs_types.h"

// /dev/null: writes are silently dropped, reads always hit EOF.
static int64_t null_read(file_t* f, void* buf, uint64_t size) {
    (void)f; (void)buf; (void)size;
    return 0;
}
static int64_t null_write(file_t* f, const void* buf, uint64_t size) {
    (void)f; (void)buf;
    return (int64_t)size;
}

static file_ops_t null_fops = {
    .read = null_read, .write = null_write,
};

// /dev/zero: writes drop, reads return `size` zero bytes.
static int64_t zero_read(file_t* f, void* buf, uint64_t size) {
    (void)f;
    if (size > 0) memset(buf, 0, size);
    return (int64_t)size;
}

static file_ops_t zero_fops = {
    .read = zero_read, .write = null_write,
};

void mem_devs_init(void) {
    devfs_register_char(1, 3, &null_fops, NULL);
    devfs_register_char(1, 5, &zero_fops, NULL);
}
