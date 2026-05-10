#include "disk_devs.h"
#include "devfs.h"
#include "pty_defs.h"
#include "vfs_types.h"
#include "vfs_defs.h"
#include "vfs_path_ops.h"
#include "vfs_path.h"
#include "vfs_mount.h"
#include "vfs_sb.h"
#include "block_device_types.h"
#include "global.h"
#include "paging.h"
#include "paging_defs.h"
#include "slab_alloc.h"
#include "string.h"
#include "vga.h"
#include "memory.h"

// Linux SCSI-disk major. 16 minors per drive: minor 0 = whole disk sda,
// minors 1..15 = sda1..sda15; minor 16 = sdb, 17..31 = sdb1..sdb15; etc.
#define DISK_MAJOR        8
#define MINORS_PER_DISK   16

// Per-node view onto a backing block device. One vector services both whole
// disks and partitions: whole disks get start_lba=0, sector_count=total.
typedef struct disk_view {
    block_device_t* bd;
    uint64_t        start_lba;
    uint64_t        sector_count;
} disk_view_t;

static uint64_t view_size_bytes(const disk_view_t* v) {
    return v->sector_count * (uint64_t)v->bd->sector_size;
}

// Byte-granular read/write through a 1-page bounce buffer. AHCI takes
// physical addresses and operates in whole sectors, so unaligned ranges
// require a copy. Mirrors the bounce-buffer pattern in ParseDevicesMbrs().
static int64_t do_io(disk_view_t* v, uint64_t byte_off, void* buf,
                     uint64_t size, bool is_write) {
    block_device_t* bd = v->bd;
    uint32_t ss = bd->sector_size;
    uint64_t cap = view_size_bytes(v);
    if (byte_off >= cap) return is_write ? -ENOSPC : 0;
    if (byte_off + size > cap) size = cap - byte_off;
    if (size == 0) return 0;

    void* bounce = (void*)AddNonCachableKernelPages(1);
    if (bounce == NULL) return -ENOMEM;
    uint64_t bounce_phys = KERNEL_VIRT_TO_PHYS((uint64_t)bounce);
    uint32_t bounce_sectors = PAGE_SIZE / ss;

    uint64_t done = 0;
    while (done < size) {
        uint64_t cur = byte_off + done;
        uint64_t rel_lba = cur / ss;
        uint32_t in_sec = (uint32_t)(cur % ss);

        uint64_t want = size - done;
        uint64_t avail_in_bounce = (uint64_t)bounce_sectors * ss - in_sec;
        if (want > avail_in_bounce) want = avail_in_bounce;

        uint32_t sectors = (uint32_t)((in_sec + want + ss - 1) / ss);
        uint64_t abs_lba = v->start_lba + rel_lba;

        if (is_write) {
            // Read-modify-write only when head/tail aren't aligned.
            bool head_partial = (in_sec != 0);
            bool tail_partial = ((in_sec + want) % ss) != 0;
            if (head_partial || tail_partial) {
                if (bd->read(bd, abs_lba, sectors, (void*)bounce_phys) != 0) {
                    RemoveKernelPages((uint64_t)bounce, 1);
                    return -EIO;
                }
            }
            memcpy((uint8_t*)bounce + in_sec, (const uint8_t*)buf + done, want);
            if (bd->write(bd, abs_lba, sectors, (void*)bounce_phys) != 0) {
                RemoveKernelPages((uint64_t)bounce, 1);
                return -EIO;
            }
        } else {
            if (bd->read(bd, abs_lba, sectors, (void*)bounce_phys) != 0) {
                RemoveKernelPages((uint64_t)bounce, 1);
                return -EIO;
            }
            memcpy((uint8_t*)buf + done, (uint8_t*)bounce + in_sec, want);
        }
        done += want;
    }
    RemoveKernelPages((uint64_t)bounce, 1);
    return (int64_t)done;
}

static int64_t disk_read(file_t* f, void* buf, uint64_t size) {
    disk_view_t* v = (disk_view_t*)f->private_data;
    int64_t r = do_io(v, f->position, buf, size, false);
    if (r > 0) f->position += (uint64_t)r;
    return r;
}

static int64_t disk_write(file_t* f, const void* buf, uint64_t size) {
    disk_view_t* v = (disk_view_t*)f->private_data;
    int64_t r = do_io(v, f->position, (void*)buf, size, true);
    if (r > 0) f->position += (uint64_t)r;
    return r;
}

static int64_t disk_seek(file_t* f, int64_t offset, int64_t whence) {
    disk_view_t* v = (disk_view_t*)f->private_data;
    int64_t base;
    switch (whence) {
        case SEEK_SET: base = 0;                          break;
        case SEEK_CUR: base = (int64_t)f->position;       break;
        case SEEK_END: base = (int64_t)view_size_bytes(v); break;
        default:       return -EINVAL;
    }
    int64_t np = base + offset;
    if (np < 0) return -EINVAL;
    f->position = (uint64_t)np;
    return np;
}

static int64_t disk_flush(file_t* f) {
    disk_view_t* v = (disk_view_t*)f->private_data;
    if (v->bd->flush == NULL) return 0;
    return v->bd->flush(v->bd) == 0 ? 0 : -EIO;
}

static file_ops_t disk_fops = {
    .read  = disk_read,
    .write = disk_write,
    .seek  = disk_seek,
    .flush = disk_flush,
};

static disk_view_t* make_view(block_device_t* bd, uint64_t start_lba, uint64_t sector_count) {
    disk_view_t* v = (disk_view_t*)kmalloc(sizeof(disk_view_t));
    if (v == NULL) return NULL;
    v->bd = bd;
    v->start_lba = start_lba;
    v->sector_count = sector_count;
    return v;
}

// True if `path` already resolves on the mounted root.
static bool path_exists(const char* path) {
    dentry_t* d = NULL;
    int64_t r = vfs_namei(path, &d);
    return r == 0 && d != NULL && d->inode != NULL;
}

// Idempotent "ensure exists": stat first; if missing, create. EEXIST is
// still treated as success in case of a race with the lookup.
static void ensure_dir(const char* path, uint64_t mode) {
    if (path_exists(path)) return;
    int64_t r = vfs_mkdir_path(path, mode);
    if (r < 0 && r != -EEXIST) {
        kprintf("disk_devs: mkdir %s failed: %d\n", path, (int)r);
    }
}

static void ensure_node(const char* path, uint64_t type, uint32_t dev_id) {
    if (path_exists(path)) return;
    int64_t r = vfs_mknod_path(path, type, 0660, dev_id);
    if (r < 0 && r != -EEXIST) {
        kprintf("disk_devs: mknod %s failed: %d\n", path, (int)r);
    }
}

// Create /dev and the fixed-name nodes whose drivers register in devfs
// during early boot (mem_devs_init, ram_block_init, tty_init). These run
// before vfs_mount_root, so the actual mknod has to happen here once the
// root filesystem is live. dev_id encoding mirrors Linux:
//   tty   c 4 0   null  c 1 3   zero  c 1 5   ram0  b 1 0
static void make_static_dev_nodes(void) {
    ensure_dir("/dev", 0755);
    ensure_node("/dev/tty",  VFS_TYPE_CHARDEV,  MKDEV(4, 0));
    ensure_node("/dev/null", VFS_TYPE_CHARDEV,  MKDEV(1, 3));
    ensure_node("/dev/zero", VFS_TYPE_CHARDEV,  MKDEV(1, 5));
    ensure_node("/dev/ram0", VFS_TYPE_BLOCKDEV, MKDEV(1, 0));

    // Pseudo-terminal multiplexor + slave nodes. Slave devices register in
    // devfs with token == &pty_table[N]; opening /dev/pts/N enters that
    // pair. /dev/fb is the framebuffer chardev for userspace mmap; /dev/kbd
    // is the raw scancode source the userspace term consumes.
    ensure_node("/dev/ptmx", VFS_TYPE_CHARDEV, MKDEV(PTY_PTMX_MAJOR, PTY_PTMX_MINOR));
    ensure_dir("/dev/pts", 0755);
    for (uint16_t i = 0; i < PTY_MAX_PAIRS; i++) {
        char path[24];
        const char* pre = "/dev/pts/";
        uint64_t pos = 0;
        while (pre[pos]) { path[pos] = pre[pos]; pos++; }
        if (i >= 10) path[pos++] = '0' + (char)(i / 10);
        path[pos++] = '0' + (char)(i % 10);
        path[pos] = '\0';
        ensure_node(path, VFS_TYPE_CHARDEV, MKDEV(PTY_SLAVE_MAJOR, i));
    }
    ensure_node("/dev/fb",  VFS_TYPE_CHARDEV, MKDEV(29, 0));
    ensure_node("/dev/kbd",   VFS_TYPE_CHARDEV, MKDEV(13, 0));
    ensure_node("/dev/mouse", VFS_TYPE_CHARDEV, MKDEV(13, 1));
    kprintf("[devs] all done\n");
}

// Count partitions belonging to a given physical device by scanning parts_head
// (the list isn't indexed). Order in the list is reverse-insertion from
// ParseMbr, so we count and then assign indices in walk order — same scheme
// PrintParitions() uses.
static void register_partitions_for(block_device_t* bd, uint32_t base_minor) {
    int64_t idx = 0;
    for (partition_device_node_t* p = parts_head; p != NULL; p = p->next) {
        if (p->value->physical_device != bd) continue;
        idx++;
        if (idx >= MINORS_PER_DISK) {
            kprintf("disk_devs: too many partitions on %s, dropping rest\n", bd->name);
            break;
        }
        partition_device_t* part = p->value;
        disk_view_t* v = make_view(bd, part->start_lba, part->sector_count);
        if (v == NULL) {
            kprintf("disk_devs: OOM allocating view for %s%d\n", bd->name, (int)idx);
            return;
        }
        uint32_t minor = base_minor + (uint32_t)idx;
        if (devfs_register_block(DISK_MAJOR, minor, &disk_fops, v) < 0) {
            kprintf("disk_devs: devfs_register_block failed for %s%d\n", bd->name, (int)idx);
            kfree(v, sizeof(disk_view_t));
            continue;
        }
        char path[32];
        // /dev/<name><idx>
        uint64_t pos = 0;
        const char* pre = "/dev/";
        while (pre[pos]) { path[pos] = pre[pos]; pos++; }
        for (uint64_t i = 0; bd->name[i] && pos < sizeof(path) - 4; i++) path[pos++] = bd->name[i];
        // append decimal idx (1..15 — single digit usually, but support 10..15)
        if (idx >= 10) path[pos++] = '0' + (char)(idx / 10);
        path[pos++] = '0' + (char)(idx % 10);
        path[pos] = '\0';
        ensure_node(path, VFS_TYPE_BLOCKDEV, MKDEV(DISK_MAJOR, minor));
        kprintf("disk_devs: registered %s (%d,%d)\n", path, DISK_MAJOR, (int)minor);
    }
}

void disk_devs_init(void) {
    make_static_dev_nodes();
    if (devices_head == NULL) {
        kprintf("disk_devs: no AHCI devices\n");
        return;
    }
    uint32_t disk_idx = 0;
    for (block_device_node_t* p = devices_head; p != NULL; p = p->next) {
        if (disk_idx >= (256 / MINORS_PER_DISK)) {
            kprintf("disk_devs: too many disks, dropping rest\n");
            break;
        }
        block_device_t* bd = p->value;
        uint32_t base_minor = disk_idx * MINORS_PER_DISK;

        disk_view_t* whole = make_view(bd, 0, bd->total_sectors);
        if (whole == NULL) {
            kprintf("disk_devs: OOM allocating view for %s\n", bd->name);
            continue;
        }
        if (devfs_register_block(DISK_MAJOR, base_minor, &disk_fops, whole) < 0) {
            kprintf("disk_devs: devfs_register_block failed for %s\n", bd->name);
            kfree(whole, sizeof(disk_view_t));
            continue;
        }
        char path[32];
        uint64_t pos = 0;
        const char* pre = "/dev/";
        while (pre[pos]) { path[pos] = pre[pos]; pos++; }
        for (uint64_t i = 0; bd->name[i] && pos < sizeof(path) - 1; i++) path[pos++] = bd->name[i];
        path[pos] = '\0';
        ensure_node(path, VFS_TYPE_BLOCKDEV, MKDEV(DISK_MAJOR, base_minor));
        kprintf("disk_devs: registered %s (%d,%d)\n", path, DISK_MAJOR, (int)base_minor);

        register_partitions_for(bd, base_minor);
        disk_idx++;
    }

    // Flush newly-created /dev nodes to disk now: kmain() never calls end(),
    // so without this any inode/dirent we just allocated stays in the buffer
    // cache and is lost when QEMU exits, leaving data.img with no /dev.
    if (vfs_root != NULL && vfs_root->inode != NULL && vfs_root->inode->sb != NULL) {
        int64_t r = vfs_sync(vfs_root->inode->sb);
        if (r < 0) kprintf("disk_devs: sync failed: %d\n", (int)r);
    }
}
