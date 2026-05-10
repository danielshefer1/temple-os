// /bin/term — userspace terminal emulator.
//
// Owns the framebuffer (via mmap of /dev/fb), the keyboard (via /dev/kbd),
// the mouse (via /dev/mouse), and a fresh pty pair (/dev/ptmx + /dev/pts/N).
// Forks a shell into the slave's stdio.
//
// All input sources funnel into one event pipe consumed by the parent's
// render loop, so selection / clipboard / cursor state all live in a single
// process (fork deep-copies anonymous pages in this OS so child-side state
// wouldn't be visible to the parent anyway).
//
//   kbd_child    --[EV_KBD]----.
//   mouse_child  --[EV_MOUSE]--+
//   pty_child    --[EV_PTY]----+--[event pipe]--> parent render loop
//   blink_child  --[EV_BLINK]--'
//   shell_child   (talks PTY slave; no direct pipe write)
//
// Frames are fixed size (sizeof(term_event_t)) so the parent can read_exact
// regardless of how the kernel chops up pipe writes.
//
// PSF2 font is embedded by the Makefile via objcopy as a binary blob.

#define ST_NO_START
#include "std/std.h"

extern const unsigned char _binary_font_psf_start[];
extern const unsigned char _binary_font_psf_end[];

// ---- PSF2 ---------------------------------------------------------------

typedef struct __attribute__((packed)) {
    unsigned char  magic[4];
    unsigned int   version;
    unsigned int   header_size;
    unsigned int   flags;
    unsigned int   num_glyphs;
    unsigned int   bytes_per_glyph;
    unsigned int   height;
    unsigned int   width;
} psf2_header_t;

// ---- VT cell + state ----------------------------------------------------

typedef struct {
    unsigned char ch;
    unsigned char fg;
    unsigned char bg;
} vt_cell_t;

static fb_var_info_t fbv;
static volatile unsigned int* fb_pixels;
static unsigned long pitch_px;

static const psf2_header_t* psf;
static const unsigned char* glyphs;
static unsigned long glyph_w, glyph_h, bytes_per_row;

static unsigned long term_cols, term_rows;
static vt_cell_t* cells;

static unsigned char cur_fg = 7;
static unsigned char cur_bg = 0;
static int           cur_bold = 0;
static unsigned long cur_row, cur_col;

// Soft block cursor overlaid on the framebuffer at (cursor_row, cursor_col).
// `cursor_visible` is set whenever the overlay is currently painted; the
// render loop erases it (via blit_cell) before drawing program output and
// repaints it after. Wipes / scrolls invalidate the tracked cell so we
// don't try to re-draw what we erased.
static int           cursor_visible;
static unsigned long cursor_row, cursor_col;

// Cursor-blink state: a child process raises SIGALRM on the parent every
// ~500ms; the handler flips `blink_on`, which interrupts the parent's
// blocking sys_read with EINTR. The render loop then either paints or
// erases the cursor depending on the current phase.
static volatile int  blink_on = 1;

// Set by SIGWINCH; the render loop consumes it to repaint cells[] over the
// framebuffer. Sent by the kernel on Alt+F1 back to vts[0] (the FB-owner
// VT) so we can stomp out whatever the previous VT painted.
static volatile int  redraw_needed = 0;

__attribute__((used))
static void winch_handler(int signo) {
    (void)signo;
    redraw_needed = 1;
}

// ---- mouse pointer + selection + clipboard -----------------------------

// Must match drivers/mouse_dev_types.h (mouse_event_t).
typedef struct {
    short         dx;
    short         dy;
    unsigned char buttons;
    unsigned char _pad[3];
} m_event_t;

static long          mx_px, my_px;          // accumulated pointer pixel pos
static unsigned long mp_row, mp_col;        // logical pointer cell
static unsigned char prev_btns;             // for press/release edge detect

static int           have_selection;        // anchor/head describe a range
static unsigned long sel_anchor;            // cell index = row*term_cols+col
static unsigned long sel_head;

// "Painted" state is what's on the framebuffer right now. Logical state
// (above) is what the next paint pass aims to put there. Tracking them
// separately lets reconcile_* compute a diff and avoid full repaints on
// every mouse event when a screen-sized selection is active.
static int           pp_visible;
static unsigned long pp_row, pp_col;
static int           sp_visible;
static unsigned long sp_lo, sp_hi;

#define CLIP_CAP 4096
static char          clipboard[CLIP_CAP];
static unsigned long clip_len;

// ---- inter-process event pipe ------------------------------------------

#define EV_KBD   1
#define EV_MOUSE 2
#define EV_PTY   3
#define EV_BLINK 4

// Fixed 64-byte frame. Reader uses read_exact so partial pipe reads can't
// desync the stream. Payload is union-typed; `len` is set for EV_PTY only.
typedef struct {
    unsigned char kind;
    unsigned char len;
    unsigned char _pad[6];
    union {
        unsigned char scancode;
        m_event_t     mouse;
        char          bytes[56];
    } p;
} term_event_t;

static int evt_w = -1;                      // write end (children)
static int evt_r = -1;                      // read end (parent)

#define VT_GROUND 0
#define VT_ESC    1
#define VT_CSI    2
static int          pstate;
static unsigned int params[8];
static int          n_params;
static int          has_param;

// CGA palette in 0xRRGGBB.
static const unsigned int cga[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

// ANSI color order -> CGA color order (same translation as the kernel VT).
static const unsigned char ansi_to_cga[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

static void my_memcpy(void* dst, const void* src, unsigned long n) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;

    // Byte-prefix until both pointers are 8-byte aligned (only possible
    // when their misalignment matches; otherwise fall back to bytewise).
    while (n && (((unsigned long)d | (unsigned long)s) & 7)) {
        if (((unsigned long)d & 7) != ((unsigned long)s & 7)) break;
        *d++ = *s++; n--;
    }

    if (n >= 8 && (((unsigned long)d | (unsigned long)s) & 7) == 0) {
        unsigned long* d64 = (unsigned long*)d;
        const unsigned long* s64 = (const unsigned long*)s;
        unsigned long qwords = n >> 3;
        for (unsigned long i = 0; i < qwords; i++) d64[i] = s64[i];
        d += qwords * 8;
        s += qwords * 8;
        n &= 7;
    }

    while (n--) *d++ = *s++;
}

// ---- framebuffer rendering ---------------------------------------------

static unsigned int pack_color(unsigned char idx) {
    unsigned int rgb = cga[idx & 0x0F];
    unsigned int r = (rgb >> 16) & 0xFF;
    unsigned int g = (rgb >> 8)  & 0xFF;
    unsigned int b =  rgb        & 0xFF;
    // 32bpp framebuffer assumed (the kernel skips fb_console for non-32bpp).
    return (r << 16) | (g << 8) | b;
}

static void blit_cell(unsigned long row, unsigned long col, vt_cell_t cell) {
    if (row >= term_rows || col >= term_cols) return;
    unsigned int fg = pack_color(cell.fg);
    unsigned int bg = pack_color(cell.bg);
    unsigned char idx = (cell.ch < 32 || cell.ch > 126) ? '?' : cell.ch;
    const unsigned char* glyph = glyphs + (unsigned long)idx * psf->bytes_per_glyph;
    unsigned long px_x = col * glyph_w;
    unsigned long px_y = row * glyph_h;
    for (unsigned long gy = 0; gy < glyph_h; gy++) {
        const unsigned char* line = glyph + gy * bytes_per_row;
        volatile unsigned int* prow = fb_pixels + (px_y + gy) * pitch_px + px_x;
        for (unsigned long gx = 0; gx < glyph_w; gx++) {
            unsigned long bit = 7 - (gx & 7);
            unsigned char byte = line[gx >> 3];
            prow[gx] = (byte & (1u << bit)) ? fg : bg;
        }
    }
}

static void fill_rect(unsigned long px_x, unsigned long px_y,
                      unsigned long w, unsigned long h, unsigned int pix) {
    for (unsigned long dy = 0; dy < h; dy++) {
        volatile unsigned int* row = fb_pixels + (px_y + dy) * pitch_px + px_x;
        for (unsigned long dx = 0; dx < w; dx++) row[dx] = pix;
    }
}

static void clear_screen(void) {
    unsigned int bg = pack_color(cur_bg);
    fill_rect(0, 0, fbv.width, fbv.height, bg);
    vt_cell_t blank = { ' ', cur_fg, cur_bg };
    for (unsigned long i = 0; i < term_rows * term_cols; i++) cells[i] = blank;
    cursor_visible = 0;
    sp_visible = 0;
    pp_visible = 0;
}

// Repaint the whole framebuffer from cells[]. Called from the render loop
// when SIGWINCH fires — vts[0] becoming active again means the kernel may
// have just blitted klog or boot-log content over us.
static void redraw_all(void) {
    fill_rect(0, 0, fbv.width, fbv.height, pack_color(cur_bg));
    for (unsigned long row = 0; row < term_rows; row++) {
        for (unsigned long col = 0; col < term_cols; col++) {
            blit_cell(row, col, cells[row * term_cols + col]);
        }
    }
    cursor_visible = 0;
    sp_visible = 0;
    pp_visible = 0;
}

// ---- overlay paint (selection + pointer + text caret) ------------------
//
// Painted state (pp_*, sp_*, cursor_*) is what's currently on the
// framebuffer. Logical state (mp_*, have_selection/sel_*, cur_*/blink_on)
// is what reconcile_* wants the framebuffer to look like. Every reconcile
// is a diff: only cells whose painted state differs from the target get
// written. That's what makes mouse drag cheap even when a screen-sized
// selection is in play — one cell is added and the rest stay put.

static void blit_inverted(unsigned long row, unsigned long col) {
    vt_cell_t c = cells[row * term_cols + col];
    vt_cell_t inv = { c.ch, c.bg, c.fg };
    blit_cell(row, col, inv);
}

static int sel_logical(unsigned long* out_lo, unsigned long* out_hi) {
    if (!have_selection) return 0;
    unsigned long lo = sel_anchor < sel_head ? sel_anchor : sel_head;
    unsigned long hi = sel_anchor < sel_head ? sel_head   : sel_anchor;
    *out_lo = lo; *out_hi = hi;
    return 1;
}

// Caret-only repaint. Restores the cell using painted state of the other
// overlays so the caret-off frame doesn't desync from the selection.
static void caret_redraw(void) {
    if (cur_row >= term_rows || cur_col >= term_cols) return;
    if (blink_on) {
        if (cursor_visible &&
            cursor_row == cur_row && cursor_col == cur_col) return;
        // Caret moved (write-position changed); erase the old solid block
        // first.
        if (cursor_visible) {
            unsigned long oidx = cursor_row * term_cols + cursor_col;
            int o_sel = sp_visible && oidx >= sp_lo && oidx <= sp_hi;
            int o_ptr = pp_visible && cursor_row == pp_row && cursor_col == pp_col;
            int o_inv = (o_ptr && o_sel) ? 0 : (o_ptr || o_sel) ? 1 : 0;
            if (o_inv) blit_inverted(cursor_row, cursor_col);
            else       blit_cell(cursor_row, cursor_col, cells[oidx]);
        }
        fill_rect(cur_col * glyph_w, cur_row * glyph_h,
                  glyph_w, glyph_h, pack_color(cur_fg));
        cursor_row = cur_row;
        cursor_col = cur_col;
        cursor_visible = 1;
    } else if (cursor_visible) {
        unsigned long idx = cursor_row * term_cols + cursor_col;
        int sel = sp_visible && idx >= sp_lo && idx <= sp_hi;
        int ptr = pp_visible && cursor_row == pp_row && cursor_col == pp_col;
        int inverted = (ptr && sel) ? 0 : (ptr || sel) ? 1 : 0;
        if (inverted) blit_inverted(cursor_row, cursor_col);
        else          blit_cell(cursor_row, cursor_col, cells[idx]);
        cursor_visible = 0;
    }
}

// Diff selection paint against logical. Touches only cells that
// transitioned (in→out or out→in), so an incremental mouse drag only
// blits the one cell that just entered or left the range.
static void reconcile_selection(void) {
    unsigned long n_lo = 0, n_hi = 0;
    int now = sel_logical(&n_lo, &n_hi);

    if (sp_visible) {
        // erase cells leaving the selection
        for (unsigned long idx = sp_lo; idx <= sp_hi; idx++) {
            int still = now && idx >= n_lo && idx <= n_hi;
            if (!still) {
                blit_cell(idx / term_cols, idx % term_cols, cells[idx]);
            }
        }
    }
    if (now) {
        // paint cells entering the selection
        for (unsigned long idx = n_lo; idx <= n_hi; idx++) {
            int was = sp_visible && idx >= sp_lo && idx <= sp_hi;
            if (!was) {
                blit_inverted(idx / term_cols, idx % term_cols);
            }
        }
        sp_visible = 1;
        sp_lo = n_lo;
        sp_hi = n_hi;
    } else {
        sp_visible = 0;
    }
}

// Restore old pointer cell (deferring to selection / caret beneath), then
// paint pointer at the logical position. Cheap: at most two cells written.
static void reconcile_pointer(void) {
    // Erase old.
    if (pp_visible) {
        if (pp_row < term_rows && pp_col < term_cols) {
            unsigned long idx = pp_row * term_cols + pp_col;
            int sel = sp_visible && idx >= sp_lo && idx <= sp_hi;
            int caret = cursor_visible && pp_row == cursor_row && pp_col == cursor_col;
            if (caret) {
                // caret block will repaint on top — nothing to do here
            } else if (sel) {
                blit_inverted(pp_row, pp_col);
            } else {
                blit_cell(pp_row, pp_col, cells[idx]);
            }
        }
        pp_visible = 0;
    }
    // Paint new.
    if (mp_row < term_rows && mp_col < term_cols) {
        unsigned long idx = mp_row * term_cols + mp_col;
        int sel = sp_visible && idx >= sp_lo && idx <= sp_hi;
        int caret = cursor_visible && mp_row == cursor_row && mp_col == cursor_col;
        if (caret) {
            // leave the caret block visible; pointer is suppressed for
            // this frame, will paint when caret blinks off.
        } else if (sel) {
            // pointer-in-selection: leave the cell uninverted so it
            // contrasts with the highlighted neighbours.
            blit_cell(mp_row, mp_col, cells[idx]);
        } else {
            blit_inverted(mp_row, mp_col);
        }
        pp_row = mp_row;
        pp_col = mp_col;
        pp_visible = 1;
    }
}

// Full from-scratch repaint of all overlays. Used after EV_PTY because
// the VT parser blit_cells over cells in normal style, destroying any
// inversion that previously covered them.
static void overlays_full_repaint(void) {
    sp_visible = 0;
    pp_visible = 0;
    cursor_visible = 0;
    reconcile_selection();
    reconcile_pointer();
    caret_redraw();
}

static void scroll_one(void) {
    // Shift cells up by one row in the backing array.
    my_memcpy(cells, cells + term_cols,
              (term_rows - 1) * term_cols * sizeof(vt_cell_t));
    vt_cell_t blank = { ' ', cur_fg, cur_bg };
    for (unsigned long c = 0; c < term_cols; c++) {
        cells[(term_rows - 1) * term_cols + c] = blank;
    }
    // Pixel-level scroll on the framebuffer. Cast volatile away for the
    // memcpy — the framebuffer is a write-combining mmap region; what we
    // need is "do the writes in some order", not "make every store
    // observable separately." The previous `dst[x] = src[x]` inner loop
    // forced one volatile read+write per pixel and dominated render time
    // (per-pixel CPU overhead, not bus). my_memcpy uses 8-byte chunks.
    unsigned int* base = (unsigned int*)fb_pixels;
    unsigned long row_px = (unsigned long)term_cols * glyph_w;
    for (unsigned long y = 0; y < (term_rows - 1) * glyph_h; y++) {
        my_memcpy(base + y * pitch_px,
                  base + (y + glyph_h) * pitch_px,
                  row_px * sizeof(unsigned int));
    }
    fill_rect(0, (term_rows - 1) * glyph_h,
              term_cols * glyph_w, glyph_h, pack_color(cur_bg));
    cursor_visible = 0;
    sp_visible = 0;
    pp_visible = 0;
    // A scroll moves all selected cells to different positions in the
    // framebuffer. Easiest correct thing is to drop the selection — the
    // user is no longer pointing at what they highlighted.
    have_selection = 0;
}

static void newline(void) {
    cur_col = 0;
    cur_row++;
    if (cur_row >= term_rows) {
        scroll_one();
        cur_row = term_rows - 1;
    }
}

static void put_glyph_at_cursor(char ch) {
    unsigned long idx = cur_row * term_cols + cur_col;
    cells[idx].ch = (unsigned char)ch;
    cells[idx].fg = cur_fg;
    cells[idx].bg = cur_bg;
    blit_cell(cur_row, cur_col, cells[idx]);
}

// ---- VT parser ---------------------------------------------------------

static unsigned int param_or(unsigned int i, unsigned int def) {
    if ((int)i >= n_params) return def;
    return params[i];
}

static void apply_sgr(void) {
    unsigned int count = (n_params == 0) ? 1 : (unsigned int)n_params;
    for (unsigned int i = 0; i < count; i++) {
        unsigned int p = ((int)i < n_params) ? params[i] : 0;
        if (p == 0)              { cur_fg = 7; cur_bg = 0; cur_bold = 0; }
        else if (p == 1)         { cur_bold = 1; }
        else if (p == 22)        { cur_bold = 0; }
        else if (p == 39)        { cur_fg = 7; }
        else if (p == 49)        { cur_bg = 0; }
        else if (p >= 30 && p <= 37)   cur_fg = ansi_to_cga[p - 30];
        else if (p >= 40 && p <= 47)   cur_bg = ansi_to_cga[p - 40];
        else if (p >= 90 && p <= 97)   cur_fg = ansi_to_cga[p - 90] | 0x08;
        else if (p >= 100 && p <= 107) cur_bg = ansi_to_cga[p - 100] | 0x08;
    }
    if (cur_bold) cur_fg |= 0x08;
}

static void clear_line_to_eol(void) {
    vt_cell_t blank = { ' ', cur_fg, cur_bg };
    for (unsigned long c = cur_col; c < term_cols; c++) {
        cells[cur_row * term_cols + c] = blank;
        blit_cell(cur_row, c, blank);
    }
}

static void dispatch_csi(char final) {
    switch (final) {
        case 'A': {
            unsigned int n = param_or(0, 1);
            if (n == 0) n = 1;
            cur_row = (cur_row > n) ? cur_row - n : 0;
            break;
        }
        case 'B': {
            unsigned int n = param_or(0, 1);
            if (n == 0) n = 1;
            cur_row += n;
            if (cur_row >= term_rows) cur_row = term_rows - 1;
            break;
        }
        case 'C': {
            unsigned int n = param_or(0, 1);
            if (n == 0) n = 1;
            cur_col += n;
            if (cur_col >= term_cols) cur_col = term_cols - 1;
            break;
        }
        case 'D': {
            unsigned int n = param_or(0, 1);
            if (n == 0) n = 1;
            cur_col = (cur_col > n) ? cur_col - n : 0;
            break;
        }
        case 'H':
        case 'f': {
            unsigned int row = param_or(0, 1);
            unsigned int col = param_or(1, 1);
            if (row == 0) row = 1;
            if (col == 0) col = 1;
            if (row > term_rows) row = term_rows;
            if (col > term_cols) col = term_cols;
            cur_row = row - 1;
            cur_col = col - 1;
            break;
        }
        case 'J': {
            unsigned int mode = param_or(0, 0);
            if (mode == 2) {
                clear_screen();
                cur_row = 0;
                cur_col = 0;
            }
            break;
        }
        case 'K':
            if (param_or(0, 0) == 0) {
                clear_line_to_eol();
            }
            break;
        case 'm':
            apply_sgr();
            break;
        default: break;
    }
}

static void reset_csi(void) {
    n_params = 0;
    has_param = 0;
    for (int i = 0; i < 8; i++) params[i] = 0;
}

static void vt_input_byte(char c) {
    switch (pstate) {
        case VT_GROUND:
            if (c == 0x1B) { pstate = VT_ESC; break; }
            switch (c) {
                case '\n': newline(); break;
                case '\r': cur_col = 0; break;
                case '\b':
                    if (cur_col > 0) {
                        cur_col--;
                        vt_cell_t blank = { ' ', cur_fg, cur_bg };
                        cells[cur_row * term_cols + cur_col] = blank;
                        blit_cell(cur_row, cur_col, blank);
                    }
                    break;
                case '\t': {
                    unsigned long next = (cur_col + 4) & ~((unsigned long)3);
                    if (next > term_cols) next = term_cols;
                    while (cur_col < next) { put_glyph_at_cursor(' '); cur_col++; }
                    break;
                }
                default:
                    put_glyph_at_cursor(c);
                    cur_col++;
                    if (cur_col >= term_cols) newline();
                    break;
            }
            break;
        case VT_ESC:
            if (c == '[') { pstate = VT_CSI; reset_csi(); }
            else          { pstate = VT_GROUND; }
            break;
        case VT_CSI:
            if (c >= '0' && c <= '9') {
                if (n_params < 8) {
                    params[n_params] = params[n_params] * 10 + (unsigned int)(c - '0');
                    has_param = 1;
                }
            } else if (c == ';') {
                if (has_param || n_params < 8) n_params++;
                has_param = 0;
            } else if (c >= 0x40 && c <= 0x7E) {
                if (has_param) n_params++;
                dispatch_csi(c);
                pstate = VT_GROUND;
            }
            break;
    }
}

// ---- scancode -> bytes -------------------------------------------------

// US scancode set 1, base layer. 0 = no ASCII (modifier or unknown).
static const char kbd_us[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t', 'q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0, ' ',
};
static const char kbd_us_shift[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t', 'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',
    0,  '*', 0, ' ',
};

#define MOD_SHIFT 1
#define MOD_CTRL  2
#define MOD_ALT   4

static unsigned char mods;
static int           pending_extended;

// Set by translate_one when Ctrl+Shift+C / Ctrl+Shift+V is pressed.
// 1 = copy, 2 = paste. Parent consumes it after each call and clears.
static int           kbd_shortcut;

// Translate one scancode byte. Returns # bytes written into out (0..6).
static int translate_one(unsigned char sc, char* out) {
    if (sc == 0xE0) { pending_extended = 1; return 0; }
    int extended = pending_extended; pending_extended = 0;
    int release = (sc & 0x80) != 0;
    unsigned char code = sc & 0x7F;

    if (code == 0x2A || code == 0x36) { // shift make/break
        if (release) mods &= ~MOD_SHIFT; else mods |= MOD_SHIFT;
        return 0;
    }
    if (code == 0x1D) { if (release) mods &= ~MOD_CTRL;  else mods |= MOD_CTRL;  return 0; }
    if (code == 0x38) { if (release) mods &= ~MOD_ALT;   else mods |= MOD_ALT;   return 0; }
    if (release) return 0;

    // Ctrl+Shift+C / Ctrl+Shift+V are clipboard shortcuts and never reach
    // the pty. Plain Ctrl+C (no shift) still folds to ^C below.
    if ((mods & (MOD_SHIFT | MOD_CTRL)) == (MOD_SHIFT | MOD_CTRL) && !extended) {
        if (code == 0x2E) { kbd_shortcut = 1; return 0; }
        if (code == 0x2F) { kbd_shortcut = 2; return 0; }
    }

    if (extended) {
        // Arrow keys, etc.
        const char* seq = 0;
        switch (code) {
            case 0x48: seq = "\x1b[A"; break;
            case 0x50: seq = "\x1b[B"; break;
            case 0x4D: seq = "\x1b[C"; break;
            case 0x4B: seq = "\x1b[D"; break;
            case 0x47: seq = "\x1b[H"; break;
            case 0x4F: seq = "\x1b[F"; break;
            default: return 0;
        }
        int n = 0; while (seq[n]) { out[n] = seq[n]; n++; }
        return n;
    }

    if (code >= 128) return 0;
    char c = (mods & MOD_SHIFT) ? kbd_us_shift[code] : kbd_us[code];
    if (c == 0) return 0;
    if (mods & MOD_CTRL) {
        if (c >= 'a' && c <= 'z') c = c - 'a' + 1;
        else if (c >= 'A' && c <= 'Z') c = c - 'A' + 1;
    }
    out[0] = c;
    return 1;
}

// ---- itoa for the pts path --------------------------------------------

static int u_to_str(unsigned int v, char* out) {
    char tmp[16]; int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v) { tmp[n++] = '0' + (char)(v % 10); v /= 10; }
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    return n;
}

// ---- clipboard ---------------------------------------------------------

// Copy the current selection to the in-process clipboard. Walks the
// selection row-by-row in reading order; trims trailing spaces of each
// row's selected sub-range; separates rows with '\n'. Silently truncates
// at CLIP_CAP bytes.
static void clipboard_copy_selection(void) {
    if (!have_selection) { clip_len = 0; return; }
    unsigned long lo = sel_anchor < sel_head ? sel_anchor : sel_head;
    unsigned long hi = sel_anchor < sel_head ? sel_head   : sel_anchor;
    unsigned long lo_row = lo / term_cols;
    unsigned long hi_row = hi / term_cols;
    unsigned long out = 0;
    for (unsigned long row = lo_row; row <= hi_row && out < CLIP_CAP; row++) {
        unsigned long c0 = (row == lo_row) ? (lo % term_cols) : 0;
        unsigned long c1 = (row == hi_row) ? (hi % term_cols) : (term_cols - 1);
        // Find last non-space column in [c0, c1].
        unsigned long last = c0;
        int any = 0;
        for (unsigned long c = c0; c <= c1; c++) {
            if (cells[row * term_cols + c].ch != ' ') { last = c; any = 1; }
        }
        unsigned long end = any ? (last + 1) : c0;
        for (unsigned long c = c0; c < end && out < CLIP_CAP; c++) {
            clipboard[out++] = (char)cells[row * term_cols + c].ch;
        }
        if (row != hi_row && out < CLIP_CAP) clipboard[out++] = '\n';
    }
    clip_len = out;
}

static void clipboard_paste(long ptmx) {
    if (clip_len == 0) return;
    unsigned long sent = 0;
    while (sent < clip_len) {
        long w = sys_write(ptmx, clipboard + sent, clip_len - sent);
        if (w <= 0) break;
        sent += (unsigned long)w;
    }
}

// ---- event-pipe framing -----------------------------------------------

static long read_exact(int fd, void* buf, unsigned long n) {
    unsigned char* p = (unsigned char*)buf;
    unsigned long got = 0;
    while (got < n) {
        long r = sys_read(fd, p + got, n - got);
        if (r == -EINTR) {
            // Only surface EINTR at a frame boundary; otherwise the caller
            // would lose `got` bytes of a half-read frame on retry.
            if (got == 0) return -EINTR;
            continue;
        }
        if (r <= 0) return r;
        got += (unsigned long)r;
    }
    return (long)got;
}

static void send_event(const term_event_t* ev) {
    // One whole frame per write — small enough (64 B) that the kernel pipe
    // ring (4 KiB) fits it atomically under uncontended conditions. If the
    // ring is congested writes may interleave at byte granularity; the
    // parent's read_exact reframes, but two writers blocked simultaneously
    // could still desync. In practice the parent drains promptly.
    const unsigned char* p = (const unsigned char*)ev;
    unsigned long left = sizeof(*ev);
    while (left) {
        long w = sys_write(evt_w, p, left);
        if (w == -EINTR) continue;
        if (w <= 0) return;
        p += w; left -= (unsigned long)w;
    }
}

// ---- mouse handling (parent side) -------------------------------------

// Returns 1 iff this event produced a visible state change (pointer cell
// moved, selection range changed, or button transitioned) — i.e. the
// render loop needs to repaint overlays. Sub-cell motion returns 0.
static int handle_mouse(const m_event_t* m) {
    unsigned long old_row = mp_row, old_col = mp_col;
    unsigned long old_anchor = sel_anchor, old_head = sel_head;
    int old_have = have_selection;

    mx_px += m->dx;
    my_px += m->dy;
    if (mx_px < 0) mx_px = 0;
    if (my_px < 0) my_px = 0;
    long max_x = (long)fbv.width - 1;
    long max_y = (long)fbv.height - 1;
    if (mx_px > max_x) mx_px = max_x;
    if (my_px > max_y) my_px = max_y;

    mp_row = (unsigned long)my_px / glyph_h;
    mp_col = (unsigned long)mx_px / glyph_w;
    if (mp_row >= term_rows) mp_row = term_rows - 1;
    if (mp_col >= term_cols) mp_col = term_cols - 1;
    unsigned long here = mp_row * term_cols + mp_col;

    unsigned char btns = m->buttons;
    unsigned char changed = btns ^ prev_btns;

    if ((changed & 1) && (btns & 1)) {
        have_selection = 1;
        sel_anchor = here;
        sel_head   = here;
    } else if (btns & 1) {
        sel_head = here;
        have_selection = 1;
    } else if ((changed & 1) && !(btns & 1)) {
        if (sel_anchor == sel_head) have_selection = 0;
    }
    prev_btns = btns;

    return (mp_row != old_row) || (mp_col != old_col) ||
           (have_selection != old_have) ||
           (have_selection &&
            (sel_anchor != old_anchor || sel_head != old_head));
}

// ---- main --------------------------------------------------------------

void _start(void) {
    long fb_fd = sys_open("/dev/fb", O_RDWR, 0);
    if (fb_fd < 0) sys_exit(1);
    if (sys_ioctl(fb_fd, FBIOGET_VSCREENINFO, &fbv) < 0) sys_exit(2);
    if (fbv.bpp != 32) sys_exit(3);
    pitch_px = fbv.pitch / 4;

    unsigned long fb_size = (unsigned long)fbv.height * fbv.pitch;
    fb_pixels = (volatile unsigned int*)sys_mmap_file(fb_fd, fb_size);
    if ((long)fb_pixels < 0 || fb_pixels == 0) sys_exit(4);

    // Parse PSF2 header.
    psf = (const psf2_header_t*)_binary_font_psf_start;
    if (psf->magic[0] != 0x72 || psf->magic[1] != 0xB5 ||
        psf->magic[2] != 0x4A || psf->magic[3] != 0x86) sys_exit(5);
    glyphs = _binary_font_psf_start + psf->header_size;
    glyph_w = psf->width;
    glyph_h = psf->height;
    bytes_per_row = (psf->width + 7) / 8;
    term_cols = fbv.width  / glyph_w;
    term_rows = fbv.height / glyph_h;

    // Cell backing buffer (sys_mmap rounds up to pages — fine).
    cells = (vt_cell_t*)sys_mmap(term_rows * term_cols * sizeof(vt_cell_t));
    if ((long)cells <= 0) sys_exit(6);
    clear_screen();

    // Start the mouse pointer at the screen centre so it's discoverable.
    mx_px = fbv.width / 2;
    my_px = fbv.height / 2;
    mp_row = my_px / glyph_h;
    mp_col = mx_px / glyph_w;

    overlays_full_repaint();

    long ptmx = sys_open("/dev/ptmx", O_RDWR, 0);
    if (ptmx < 0) sys_exit(7);
    unsigned int n = 0;
    sys_ioctl(ptmx, TIOCGPTN, &n);
    int unlock = 0;
    sys_ioctl(ptmx, TIOCSPTLCK, &unlock);

    char pts_path[24];
    const char* pre = "/dev/pts/";
    int pos = 0; while (pre[pos]) { pts_path[pos] = pre[pos]; pos++; }
    pos += u_to_str(n, pts_path + pos);
    pts_path[pos] = '\0';

    // Initial winsize.
    winsize_t ws = { (unsigned short)term_rows, (unsigned short)term_cols,
                     (unsigned short)fbv.width, (unsigned short)fbv.height };
    sys_ioctl(ptmx, TIOCSWINSZ, &ws);

    // Fork the program-to-run.
    long shell_pid = sys_fork();
    if (shell_pid == 0) {
        sys_setsid();
        long slave = sys_open(pts_path, O_RDWR, 0);
        if (slave < 0) sys_exit(20);
        sys_ioctl(slave, TIOCSCTTY, 0);
        sys_dup2(slave, 0);
        sys_dup2(slave, 1);
        sys_dup2(slave, 2);
        if (slave > 2) sys_close(slave);
        sys_close(ptmx);
        sys_close(fb_fd);
        char* const sh_argv[] = { "sh", 0 };
        sys_exec("/bin/sh", sh_argv, 0);
        sys_exit(127);
    }

    // One pipe carries every input source to the parent's render loop.
    int evt_fds[2];
    if (sys_pipe(evt_fds) < 0) sys_exit(31);
    evt_r = evt_fds[0];
    evt_w = evt_fds[1];

    sys_signal(SIGWINCH, (void*)winch_handler, (void*)sig_restorer);

    // kbd_child: scancodes -> EV_KBD frames.
    long kbd_pid = sys_fork();
    if (kbd_pid == 0) {
        sys_close(fb_fd);
        sys_close(ptmx);
        sys_close(evt_r);
        long kbd_fd = sys_open("/dev/kbd", O_RDONLY, 0);
        if (kbd_fd < 0) sys_exit(30);
        term_event_t ev;
        unsigned char sc;
        while (1) {
            long r = sys_read(kbd_fd, &sc, 1);
            if (r <= 0) break;
            ev.kind = EV_KBD; ev.len = 1; ev.p.scancode = sc;
            send_event(&ev);
        }
        sys_exit(0);
    }

    // mouse_child: relative deltas -> EV_MOUSE frames.
    long mouse_pid = sys_fork();
    if (mouse_pid == 0) {
        sys_close(fb_fd);
        sys_close(ptmx);
        sys_close(evt_r);
        long mfd = sys_open("/dev/mouse", O_RDONLY, 0);
        if (mfd < 0) sys_exit(32);
        term_event_t ev;
        m_event_t in;
        while (1) {
            long r = sys_read(mfd, &in, sizeof(in));
            if (r <= 0) break;
            if (r != (long)sizeof(in)) continue;  // short read: drop
            ev.kind = EV_MOUSE; ev.len = sizeof(in); ev.p.mouse = in;
            send_event(&ev);
        }
        sys_exit(0);
    }

    // pty_child: shell output -> EV_PTY frames.
    long pty_pid = sys_fork();
    if (pty_pid == 0) {
        sys_close(fb_fd);
        sys_close(evt_r);
        term_event_t ev;
        while (1) {
            long r = sys_read(ptmx, ev.p.bytes, sizeof(ev.p.bytes));
            if (r == -EINTR) continue;
            if (r <= 0) break;
            ev.kind = EV_PTY;
            ev.len = (unsigned char)r;
            send_event(&ev);
        }
        sys_exit(0);
    }

    // blink_child: periodic EV_BLINK frames (replaces SIGALRM).
    long blink_pid = sys_fork();
    if (blink_pid == 0) {
        sys_close(fb_fd);
        sys_close(ptmx);
        sys_close(evt_r);
        term_event_t ev;
        ev.kind = EV_BLINK; ev.len = 0;
        for (;;) {
            sys_sleep(500);
            send_event(&ev);
        }
    }

    // Parent owns ptmx (for writes) and the event-pipe read end.
    sys_close(evt_w);
    evt_w = -1;

    // Render loop.
    term_event_t ev;
    while (1) {
        long r = read_exact(evt_r, &ev, sizeof(ev));
        if (r == -EINTR) {
            if (redraw_needed) {
                redraw_needed = 0;
                redraw_all();
                blink_on = 1;
                overlays_full_repaint();
            }
            continue;
        }
        if (r <= 0) break;

        switch (ev.kind) {
            case EV_PTY: {
                // VT parser will blit_cell underneath the overlay paint;
                // do a full overlay repaint afterwards to re-invert any
                // selected cells that got rewritten.
                for (unsigned int i = 0; i < ev.len; i++) {
                    vt_input_byte(ev.p.bytes[i]);
                }
                // Reset the blink to ON after activity so the caret
                // always reappears at the new write position immediately.
                blink_on = 1;
                overlays_full_repaint();
                break;
            }
            case EV_KBD: {
                // Keyboard work never changes the framebuffer directly;
                // shell echo comes back as EV_PTY and triggers a repaint
                // there. Skipping the overlay flip keeps Ctrl auto-repeat
                // (which can fire at >30 Hz) from thrashing big selections.
                char out[8];
                kbd_shortcut = 0;
                int len = translate_one(ev.p.scancode, out);
                if (kbd_shortcut == 1) {
                    clipboard_copy_selection();
                } else if (kbd_shortcut == 2) {
                    clipboard_paste(ptmx);
                } else if (len > 0) {
                    sys_write(ptmx, out, len);
                }
                break;
            }
            case EV_MOUSE: {
                if (handle_mouse(&ev.p.mouse)) {
                    // Diff-update: at most O(|added| + |removed|) cells
                    // for selection plus 2 for pointer. Mouse drag across
                    // a screen-sized selection stays cheap because the
                    // diff per event is one cell.
                    reconcile_selection();
                    reconcile_pointer();
                }
                break;
            }
            case EV_BLINK: {
                blink_on = !blink_on;
                caret_redraw();
                break;
            }
            default: break;
        }
    }

    sys_kill(kbd_pid,   SIGTERM);
    sys_kill(mouse_pid, SIGTERM);
    sys_kill(pty_pid,   SIGTERM);
    sys_kill(blink_pid, SIGTERM);
    sys_exit(0);
}
