#include "fb.h"
#include "global.h"
#include "paging.h"
#include "paging_defs.h"
#include "vga.h"

void fb_map(void) {
    if (fb_info.fb_phys == 0 || fb_info.size == 0) {
        kprintf("fb: no framebuffer captured from Limine\n");
        return;
    }

    // FB physical base may not be 2MB aligned; map at 4KB granularity. Use
    // RW_FB (PAT slot 1 = WC) instead of RW_MMIO (UC) so pixel stores coalesce
    // into 64-byte burst transactions; this is what makes vt scroll/blit not
    // stall on the cache-line write path. pat_init must have run on this CPU
    // before any access through these mappings.
    uint64_t pages = fb_info.size / PAGE_SIZE;
    for (uint64_t i = 0; i < pages; i++) {
        map_page_to_virt(FB_VIRTUAL + i * PAGE_SIZE,
                         fb_info.fb_phys + i * PAGE_SIZE,
                         RW_FB, false);
    }
    fb_info.fb_virt = FB_VIRTUAL;
}

uint32_t fb_pack(uint8_t r, uint8_t g, uint8_t b) {
    // Mask sizes from Limine are typically 8/8/8 for RGB. Clip just in case.
    uint32_t rm = ((1u << fb_info.red_size)   - 1u);
    uint32_t gm = ((1u << fb_info.green_size) - 1u);
    uint32_t bm = ((1u << fb_info.blue_size)  - 1u);
    uint32_t rv = ((uint32_t)r & rm) << fb_info.red_shift;
    uint32_t gv = ((uint32_t)g & gm) << fb_info.green_shift;
    uint32_t bv = ((uint32_t)b & bm) << fb_info.blue_shift;
    return rv | gv | bv;
}

void fb_clear(uint32_t pixel) {
    if (fb_info.fb_virt == 0) return;
    if (fb_info.bpp != 32) return;     // M1 only supports 32bpp; M3 will widen.

    volatile uint32_t* row = (volatile uint32_t*)fb_info.fb_virt;
    uint64_t pixels_per_row = fb_info.pitch / 4;
    for (uint64_t y = 0; y < fb_info.height; y++) {
        for (uint64_t x = 0; x < fb_info.width; x++) {
            row[x] = pixel;
        }
        row += pixels_per_row;
    }
}

// M1 sanity pattern: solid red background, green vertical bar in the middle
// third, blue horizontal bar in the middle third. If the channels are wrong
// (e.g. BGR vs RGB) the colors will be visibly swapped.
void fb_test_pattern(void) {
    if (fb_info.fb_virt == 0 || fb_info.bpp != 32) return;
    uint32_t red   = fb_pack(0xFF, 0x00, 0x00);
    uint32_t green = fb_pack(0x00, 0xFF, 0x00);
    uint32_t blue  = fb_pack(0x00, 0x00, 0xFF);

    uint64_t pitch_px = fb_info.pitch / 4;
    uint64_t x_lo = fb_info.width  / 3, x_hi = (fb_info.width  * 2) / 3;
    uint64_t y_lo = fb_info.height / 3, y_hi = (fb_info.height * 2) / 3;

    volatile uint32_t* fb = (volatile uint32_t*)fb_info.fb_virt;
    for (uint64_t y = 0; y < fb_info.height; y++) {
        for (uint64_t x = 0; x < fb_info.width; x++) {
            uint32_t c = red;
            if (y >= y_lo && y < y_hi)      c = blue;
            else if (x >= x_lo && x < x_hi) c = green;
            fb[y * pitch_px + x] = c;
        }
    }
}
