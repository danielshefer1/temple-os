#include "fb_dev.h"
#include "devfs.h"
#include "vfs_types.h"
#include "vfs_defs.h"
#include "global.h"
#include "paging_defs.h"
#include "string.h"

static int64_t fb_dev_ioctl(file_t* f, uint64_t cmd, void* arg) {
    (void)f;
    if (cmd != FBIOGET_VSCREENINFO) return -ENOTTY;
    if (!arg) return -EINVAL;
    fb_var_info_t* v = (fb_var_info_t*)arg;
    v->width  = (uint32_t)fb_info.width;
    v->height = (uint32_t)fb_info.height;
    v->pitch  = (uint32_t)fb_info.pitch;
    v->bpp    = (uint32_t)fb_info.bpp;
    return 0;
}

// mmap_phys: hand back the framebuffer's physical page at the requested
// offset. The kernel mmap path uses this to populate a user pml4 with the
// device's physical pages — no copy, writes hit the device directly.
static int64_t fb_dev_mmap_phys(file_t* f, uint64_t offset, uint64_t* phys_out) {
    (void)f;
    if (!phys_out) return -EINVAL;
    if (offset & (PAGE_SIZE - 1)) return -EINVAL;
    if (fb_info.fb_phys == 0 || fb_info.size == 0) return -ENODEV;
    if (offset >= fb_info.size) return -EINVAL;
    *phys_out = fb_info.fb_phys + offset;
    return 0;
}

static int64_t fb_dev_close(file_t* f) { (void)f; return 0; }

static file_ops_t fb_dev_fops = {
    .ioctl     = fb_dev_ioctl,
    .mmap_phys = fb_dev_mmap_phys,
    .close     = fb_dev_close,
};

void fb_dev_init(void) {
    devfs_register_char(29, 0, &fb_dev_fops, NULL);
}
