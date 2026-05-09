#include "fb_console.h"
#include "fb.h"
#include "global.h"
#include "vga.h"

// PSF2 binary embedded by the Makefile via objcopy.
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

static const uint32_t cga_palette_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static const psf2_header_t* g_hdr;
static const uint8_t* g_glyphs;
static uint64_t g_glyph_w;
static uint64_t g_glyph_h;
static uint64_t g_bytes_per_row;   // glyph data bytes per pixel-row of one cell
static uint64_t g_cols;
static uint64_t g_rows;
static bool     g_ready;

static inline uint32_t* fb_ptr_at(uint64_t px_x, uint64_t px_y) {
    uint64_t pitch_px = fb_info.pitch / 4;
    return (uint32_t*)fb_info.fb_virt + px_y * pitch_px + px_x;
}

static uint32_t pack_palette(uint8_t idx) {
    uint32_t rgb = cga_palette_rgb[idx & 0x0F];
    return fb_pack((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

static void fill_rect(uint64_t px_x, uint64_t px_y,
                      uint64_t w, uint64_t h, uint32_t pixel) {
    for (uint64_t dy = 0; dy < h; dy++) {
        volatile uint32_t* row = (volatile uint32_t*)fb_ptr_at(px_x, px_y + dy);
        for (uint64_t dx = 0; dx < w; dx++) row[dx] = pixel;
    }
}

void fb_blit_cell(uint64_t row, uint64_t col, vt_cell_t cell) {
    if (!g_ready) return;
    if (row >= g_rows || col >= g_cols) return;

    uint32_t fg = pack_palette(cell.fg);
    uint32_t bg = pack_palette(cell.bg);

    // Render only printable ASCII; replace control / non-ASCII bytes with
    // '?' so we never read past the 256-glyph slab into the PSF2 Unicode
    // table.
    uint8_t glyph_idx = ((uint8_t)cell.ch < 32 || (uint8_t)cell.ch > 126)
        ? '?' : (uint8_t)cell.ch;
    const uint8_t* glyph = g_glyphs + (uint64_t)glyph_idx * g_hdr->bytes_per_glyph;

    uint64_t px_x = col * g_glyph_w;
    uint64_t px_y = row * g_glyph_h;
    uint64_t pitch_px = fb_info.pitch / 4;
    volatile uint32_t* row0 = (volatile uint32_t*)fb_ptr_at(px_x, px_y);

    for (uint64_t gy = 0; gy < g_glyph_h; gy++) {
        const uint8_t* line_bytes = glyph + gy * g_bytes_per_row;
        volatile uint32_t* prow = row0 + gy * pitch_px;
        for (uint64_t gx = 0; gx < g_glyph_w; gx++) {
            uint64_t bit = 7 - (gx & 7);
            uint8_t  byte = line_bytes[gx >> 3];
            prow[gx] = (byte & (1u << bit)) ? fg : bg;
        }
    }
}

void fb_clear_all(uint8_t bg) {
    if (!g_ready) return;
    fill_rect(0, 0, fb_info.width, fb_info.height, pack_palette(bg));
}

void fb_scroll_up(uint64_t cell_rows, uint64_t cell_cols, uint8_t bg) {
    if (!g_ready) return;
    uint64_t row_bytes    = fb_info.pitch * g_glyph_h;
    uint64_t scroll_bytes = fb_info.pitch * (cell_rows - 1) * g_glyph_h;
    uint8_t* fb_base = (uint8_t*)fb_info.fb_virt;
    // Forward copy is safe (dst < src) for upward scroll.
    memcpy(fb_base, fb_base + row_bytes, scroll_bytes);
    fill_rect(0, (cell_rows - 1) * g_glyph_h,
              cell_cols * g_glyph_w, g_glyph_h, pack_palette(bg));
}

void fb_redraw_cells(const vt_cell_t* cells, uint64_t rows, uint64_t cols) {
    if (!g_ready) return;
    for (uint64_t r = 0; r < rows; r++) {
        for (uint64_t c = 0; c < cols; c++) {
            fb_blit_cell(r, c, cells[r * cols + c]);
        }
    }
}

bool fb_console_ready(void) { return g_ready; }

bool fb_console_init(uint64_t* out_cols, uint64_t* out_rows) {
    if (out_cols) *out_cols = 0;
    if (out_rows) *out_rows = 0;

    if (fb_info.fb_virt == 0 || fb_info.bpp != 32) {
        kprintf("fb_console: skipped (no FB or non-32bpp)\n");
        return false;
    }

    const psf2_header_t* hdr = (const psf2_header_t*)_binary_font_psf_start;
    if (hdr->magic[0] != PSF2_MAGIC0 || hdr->magic[1] != PSF2_MAGIC1 ||
        hdr->magic[2] != PSF2_MAGIC2 || hdr->magic[3] != PSF2_MAGIC3) {
        kprintf("fb_console: bad PSF2 magic\n");
        return false;
    }
    if (hdr->width == 0 || hdr->height == 0 || hdr->bytes_per_glyph == 0) {
        kprintf("fb_console: degenerate font dims\n");
        return false;
    }

    g_hdr           = hdr;
    g_glyphs        = _binary_font_psf_start + hdr->header_size;
    g_glyph_w       = hdr->width;
    g_glyph_h       = hdr->height;
    g_bytes_per_row = (hdr->width + 7) / 8;
    g_cols          = fb_info.width  / g_glyph_w;
    g_rows          = fb_info.height / g_glyph_h;
    g_ready         = true;

    fb_clear_all(0);

    if (out_cols) *out_cols = g_cols;
    if (out_rows) *out_rows = g_rows;
    kprintf("fb_console: %dx%d glyphs (%dx%d px each)\n",
            g_cols, g_rows, g_glyph_w, g_glyph_h);
    return true;
}
