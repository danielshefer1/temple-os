#include "fb_console.h"
#include "console.h"
#include "fb.h"
#include "global.h"
#include "vga.h"
#include "string.h"
#include "lock_types.h"
#include "extern.h"

// PSF2 binary embedded by `objcopy --rename-section .data=.rodata` in the
// Makefile. The symbols are emitted by objcopy from the input filename
// (assets/font.psf, but we cd into assets/ before invoking it so the
// symbol stems are font_psf, not assets_font_psf).
extern const uint8_t _binary_font_psf_start[];
extern const uint8_t _binary_font_psf_end[];

#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xB5
#define PSF2_MAGIC2 0x4A
#define PSF2_MAGIC3 0x86

typedef struct __attribute__((packed)) {
    uint8_t  magic[4];
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint32_t num_glyphs;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
} psf2_header_t;

// Standard 16-color CGA palette mapped to RGB. Indices match the low nibble
// of CGA attribute bytes (so GREY_COLOR=0x07 picks LightGrey).
static const uint32_t cga_palette_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static struct {
    spinlock_t lock;
    const psf2_header_t* hdr;
    const uint8_t* glyphs;
    uint64_t glyph_w;
    uint64_t glyph_h;
    uint64_t bytes_per_row;   // bytes of glyph data per pixel-row of one char
    uint64_t cols;
    uint64_t rows;
    uint64_t cur_row;
    uint64_t cur_col;
    uint8_t  fg_idx;          // index into cga_palette_rgb
    uint8_t  bg_idx;
    uint32_t fg_pixel;        // pre-packed for the current FB layout
    uint32_t bg_pixel;
} fbc;

static console_t fb_console_dev;

// ---- low-level pixel/glyph helpers ----------------------------------------

static inline uint32_t* fb_ptr_at(uint64_t px_x, uint64_t px_y) {
    uint64_t pitch_px = fb_info.pitch / 4;
    return (uint32_t*)fb_info.fb_virt + px_y * pitch_px + px_x;
}

static void fill_rect(uint64_t px_x, uint64_t px_y,
                      uint64_t w, uint64_t h, uint32_t pixel) {
    for (uint64_t dy = 0; dy < h; dy++) {
        volatile uint32_t* row = (volatile uint32_t*)fb_ptr_at(px_x, px_y + dy);
        for (uint64_t dx = 0; dx < w; dx++) row[dx] = pixel;
    }
}

static void draw_glyph(uint64_t cell_col, uint64_t cell_row, uint8_t ch) {
    const uint8_t* glyph = fbc.glyphs + (uint64_t)ch * fbc.hdr->bytes_per_glyph;
    uint64_t px_x = cell_col * fbc.glyph_w;
    uint64_t px_y = cell_row * fbc.glyph_h;
    uint64_t pitch_px = fb_info.pitch / 4;
    volatile uint32_t* row0 = (volatile uint32_t*)fb_ptr_at(px_x, px_y);

    for (uint64_t gy = 0; gy < fbc.glyph_h; gy++) {
        const uint8_t* line_bytes = glyph + gy * fbc.bytes_per_row;
        volatile uint32_t* row = row0 + gy * pitch_px;
        for (uint64_t gx = 0; gx < fbc.glyph_w; gx++) {
            uint64_t bit = 7 - (gx & 7);     // bit 7 = leftmost
            uint8_t  byte = line_bytes[gx >> 3];
            row[gx] = (byte & (1u << bit)) ? fbc.fg_pixel : fbc.bg_pixel;
        }
    }
}

static void scroll_one_row(void) {
    uint64_t row_bytes = fb_info.pitch * fbc.glyph_h;
    uint64_t scroll_bytes = fb_info.pitch * (fbc.rows - 1) * fbc.glyph_h;
    uint8_t* fb_base = (uint8_t*)fb_info.fb_virt;

    // Forward copy is safe: dst < src for an upward scroll.
    memcpy(fb_base, fb_base + row_bytes, scroll_bytes);
    fill_rect(0, (fbc.rows - 1) * fbc.glyph_h,
              fbc.cols * fbc.glyph_w, fbc.glyph_h, fbc.bg_pixel);
}

static void newline_locked(void) {
    fbc.cur_col = 0;
    fbc.cur_row++;
    if (fbc.cur_row >= fbc.rows) {
        scroll_one_row();
        fbc.cur_row = fbc.rows - 1;
    }
}

// ---- console_t vtable -----------------------------------------------------

static void fb_putc_impl(console_t* c, char ch) {
    (void)c;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&fbc.lock);

    switch (ch) {
        case '\n': newline_locked(); break;
        case '\r': fbc.cur_col = 0; break;
        case '\b':
            if (fbc.cur_col > 0) {
                fbc.cur_col--;
                draw_glyph(fbc.cur_col, fbc.cur_row, ' ');
            }
            break;
        case '\t': {
            uint64_t next = (fbc.cur_col + 4) & ~((uint64_t)3);
            if (next > fbc.cols) next = fbc.cols;
            while (fbc.cur_col < next) {
                draw_glyph(fbc.cur_col, fbc.cur_row, ' ');
                fbc.cur_col++;
            }
            break;
        }
        default: {
            // Render only ASCII printable + plain whitespace; non-ASCII falls
            // back to '?' so user data can't drive us into the Unicode table
            // half of the PSF2 file.
            uint8_t glyph_ch = ((uint8_t)ch < 32 || (uint8_t)ch > 126) ? '?' : (uint8_t)ch;
            draw_glyph(fbc.cur_col, fbc.cur_row, glyph_ch);
            fbc.cur_col++;
            if (fbc.cur_col >= fbc.cols) newline_locked();
            break;
        }
    }

    spin_unlock(&fbc.lock);
    if (ie) StiHelper();
}

static void fb_clear_impl(console_t* c) {
    (void)c;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&fbc.lock);
    fill_rect(0, 0, fbc.cols * fbc.glyph_w, fbc.rows * fbc.glyph_h, fbc.bg_pixel);
    fbc.cur_row = 0;
    fbc.cur_col = 0;
    spin_unlock(&fbc.lock);
    if (ie) StiHelper();
}

static uint32_t pack_palette(uint8_t idx) {
    uint32_t rgb = cga_palette_rgb[idx & 0x0F];
    return fb_pack((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

static void fb_set_attr_impl(console_t* c, uint8_t fg, uint8_t bg) {
    (void)c;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&fbc.lock);
    if (fg != fbc.fg_idx) { fbc.fg_idx = fg & 0x0F; fbc.fg_pixel = pack_palette(fbc.fg_idx); }
    if (bg != fbc.bg_idx) { fbc.bg_idx = bg & 0x0F; fbc.bg_pixel = pack_palette(fbc.bg_idx); }
    spin_unlock(&fbc.lock);
    if (ie) StiHelper();
}

static void fb_set_cursor_impl(console_t* c, uint64_t row, uint64_t col) {
    (void)c;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&fbc.lock);
    if (row < fbc.rows && col < fbc.cols) {
        fbc.cur_row = row;
        fbc.cur_col = col;
    }
    spin_unlock(&fbc.lock);
    if (ie) StiHelper();
}

// ---- public init ----------------------------------------------------------

void fb_console_init(void) {
    if (fb_info.fb_virt == 0 || fb_info.bpp != 32) {
        kprintf("fb_console: skipped (no FB or non-32bpp)\n");
        return;
    }

    const psf2_header_t* hdr = (const psf2_header_t*)_binary_font_psf_start;
    if (hdr->magic[0] != PSF2_MAGIC0 || hdr->magic[1] != PSF2_MAGIC1 ||
        hdr->magic[2] != PSF2_MAGIC2 || hdr->magic[3] != PSF2_MAGIC3) {
        kprintf("fb_console: bad PSF2 magic %x %x %x %x\n",
                (uint64_t)hdr->magic[0], (uint64_t)hdr->magic[1],
                (uint64_t)hdr->magic[2], (uint64_t)hdr->magic[3]);
        return;
    }
    if (hdr->width == 0 || hdr->height == 0 || hdr->bytes_per_glyph == 0) {
        kprintf("fb_console: degenerate font dims\n");
        return;
    }

    fbc.hdr = hdr;
    fbc.glyphs = _binary_font_psf_start + hdr->header_size;
    fbc.glyph_w = hdr->width;
    fbc.glyph_h = hdr->height;
    fbc.bytes_per_row = (hdr->width + 7) / 8;
    fbc.cols = fb_info.width  / fbc.glyph_w;
    fbc.rows = fb_info.height / fbc.glyph_h;
    fbc.cur_row = 0;
    fbc.cur_col = 0;
    fbc.fg_idx = 0x07;   // light grey on black, matching GREY_COLOR
    fbc.bg_idx = 0x00;
    fbc.fg_pixel = pack_palette(fbc.fg_idx);
    fbc.bg_pixel = pack_palette(fbc.bg_idx);

    fb_console_dev.putc       = fb_putc_impl;
    fb_console_dev.clear      = fb_clear_impl;
    fb_console_dev.set_attr   = fb_set_attr_impl;
    fb_console_dev.set_cursor = fb_set_cursor_impl;
    fb_console_dev.rows       = fbc.rows;
    fbc.lock.locked = 0;
    fb_console_dev.cols       = fbc.cols;
    fb_console_dev.priv       = &fbc;

    // Clear before registering so the first kprintf after this paints a clean
    // surface rather than streaming into whatever Limine left behind.
    fill_rect(0, 0, fb_info.width, fb_info.height, fbc.bg_pixel);

    console_register(&fb_console_dev);
    kprintf("fb_console: %dx%d glyphs (%dx%d px each)\n",
            fbc.cols, fbc.rows, fbc.glyph_w, fbc.glyph_h);
}
