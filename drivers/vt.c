#include "vt.h"
#include "fb_console.h"
#include "global.h"
#include "vga.h"
#include "buddy_alloc.h"
#include "paging_defs.h"
#include "string.h"
#include "extern.h"
#include "signal.h"
#include "signal_defs.h"

#define DEFAULT_FG 0x07
#define DEFAULT_BG 0x00

vt_t   vts[NUM_VTS];
vt_t*  active_vt = NULL;
vt_t*  klog_vt   = NULL;

// Userspace task that owns /dev/fb (term, today). When the active VT lands on
// vts[0] — the boot/term VT — and an owner is set, vt_switch_to wakes it with
// SIGWINCH so it can repaint cells[] over whatever the previous VT painted,
// instead of having the stale boot-log backbuffer blitted over its output.
task_t* fb_owner = NULL;

// ANSI/VT SGR color codes use RGB ordering (31=red, 34=blue), CGA uses BGR
// (1=blue, 4=red). Same translation as the old vt_parser.
static const uint8_t ansi_to_cga[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

// ---- backbuffer helpers (caller must hold vt->lock) -----------------------

static inline vt_cell_t* cell_at(vt_t* vt, uint64_t row, uint64_t col) {
    return &vt->cells[row * vt->cols + col];
}

static void clear_cells(vt_t* vt) {
    vt_cell_t blank = { .ch = ' ', .fg = vt->cur_fg, .bg = vt->cur_bg };
    for (uint64_t i = 0; i < vt->rows * vt->cols; i++) vt->cells[i] = blank;
}

static void scroll_one(vt_t* vt) {
    // Shift cells up by one row.
    uint64_t row_cells = vt->cols;
    // Forward copy is safe: dst < src for an upward scroll.
    memcpy(vt->cells, vt->cells + row_cells,
           (vt->rows - 1) * row_cells * sizeof(vt_cell_t));
    vt_cell_t blank = { .ch = ' ', .fg = vt->cur_fg, .bg = vt->cur_bg };
    for (uint64_t c = 0; c < vt->cols; c++) {
        vt->cells[(vt->rows - 1) * vt->cols + c] = blank;
    }
    if (vt == active_vt) {
        fb_scroll_up(vt->cells, vt->rows, vt->cols, vt->cur_bg);
    }
}

static void put_glyph(vt_t* vt, char ch) {
    vt_cell_t cell = { .ch = ch, .fg = vt->cur_fg, .bg = vt->cur_bg };
    *cell_at(vt, vt->cur_row, vt->cur_col) = cell;
    if (vt == active_vt) fb_blit_cell(vt->cur_row, vt->cur_col, cell);
}

static void newline_locked(vt_t* vt) {
    vt->cur_col = 0;
    vt->cur_row++;
    if (vt->cur_row >= vt->rows) {
        scroll_one(vt);
        vt->cur_row = vt->rows - 1;
    }
}

// ---- parser dispatch ------------------------------------------------------

static uint32_t param_or(vt_t* vt, uint32_t idx, uint32_t def) {
    if (idx >= vt->n_params) return def;
    return vt->params[idx];
}

static void apply_sgr(vt_t* vt) {
    uint32_t count = vt->n_params == 0 ? 1 : vt->n_params;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t p = (i < vt->n_params) ? vt->params[i] : 0;
        if (p == 0) {
            vt->cur_fg = DEFAULT_FG;
            vt->cur_bg = DEFAULT_BG;
            vt->cur_bold = false;
        } else if (p == 1) {
            vt->cur_bold = true;
        } else if (p == 22) {
            vt->cur_bold = false;
        } else if (p == 39) {
            vt->cur_fg = DEFAULT_FG;
        } else if (p == 49) {
            vt->cur_bg = DEFAULT_BG;
        } else if (p >= 30 && p <= 37) {
            vt->cur_fg = ansi_to_cga[p - 30];
        } else if (p >= 40 && p <= 47) {
            vt->cur_bg = ansi_to_cga[p - 40];
        } else if (p >= 90 && p <= 97) {
            vt->cur_fg = (uint8_t)(ansi_to_cga[p - 90] | 0x08);
        } else if (p >= 100 && p <= 107) {
            vt->cur_bg = (uint8_t)(ansi_to_cga[p - 100] | 0x08);
        }
    }
    if (vt->cur_bold) vt->cur_fg |= 0x08;
}

static void dispatch_csi(vt_t* vt, char final) {
    switch (final) {
        case 'H':
        case 'f': {
            uint32_t row = param_or(vt, 0, 1);
            uint32_t col = param_or(vt, 1, 1);
            if (row == 0) row = 1;
            if (col == 0) col = 1;
            if (row > vt->rows) row = vt->rows;
            if (col > vt->cols) col = vt->cols;
            vt->cur_row = row - 1;
            vt->cur_col = col - 1;
            break;
        }
        case 'J': {
            uint32_t mode = param_or(vt, 0, 0);
            if (mode == 2) {
                clear_cells(vt);
                vt->cur_row = 0;
                vt->cur_col = 0;
                if (vt == active_vt) fb_clear_all(vt->cur_bg);
            }
            // Modes 0/1 (partial) not implemented — fall through silently.
            break;
        }
        case 'K': {
            // EL — clear current line. Mode 0 (cursor to end) is the only one
            // we implement; covers the common readline-style use case.
            uint32_t mode = param_or(vt, 0, 0);
            if (mode == 0) {
                vt_cell_t blank = { .ch = ' ', .fg = vt->cur_fg, .bg = vt->cur_bg };
                for (uint64_t c = vt->cur_col; c < vt->cols; c++) {
                    *cell_at(vt, vt->cur_row, c) = blank;
                    if (vt == active_vt) fb_blit_cell(vt->cur_row, c, blank);
                }
            }
            break;
        }
        case 'm':
            apply_sgr(vt);
            break;
        // Relative cursor moves still stubbed — we now have row/col on the VT,
        // so adding these is straightforward and will land when readline lands.
        default:
            break;
    }
}

static void reset_csi(vt_t* vt) {
    vt->n_params = 0;
    vt->has_param = false;
    for (uint32_t i = 0; i < VT_MAX_PARAMS; i++) vt->params[i] = 0;
}

// ---- public entry points --------------------------------------------------

void vt_write_byte(vt_t* vt, char c) {
    if (vt == NULL || vt->cells == NULL) return;
    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&vt->lock);

    switch (vt->pstate) {
        case VT_GROUND:
            if (c == 0x1B) { vt->pstate = VT_ESC; break; }
            switch (c) {
                case '\n':
                    newline_locked(vt);
                    break;
                case '\r':
                    vt->cur_col = 0;
                    break;
                case '\b':
                    if (vt->cur_col > 0) {
                        vt->cur_col--;
                        vt_cell_t blank = { .ch = ' ', .fg = vt->cur_fg, .bg = vt->cur_bg };
                        *cell_at(vt, vt->cur_row, vt->cur_col) = blank;
                        if (vt == active_vt) fb_blit_cell(vt->cur_row, vt->cur_col, blank);
                    }
                    break;
                case '\t': {
                    uint64_t next = (vt->cur_col + 4) & ~((uint64_t)3);
                    if (next > vt->cols) next = vt->cols;
                    while (vt->cur_col < next) {
                        put_glyph(vt, ' ');
                        vt->cur_col++;
                    }
                    break;
                }
                default:
                    put_glyph(vt, c);
                    vt->cur_col++;
                    if (vt->cur_col >= vt->cols) newline_locked(vt);
                    break;
            }
            break;

        case VT_ESC:
            if (c == '[') { vt->pstate = VT_CSI; reset_csi(vt); }
            else          { vt->pstate = VT_GROUND; }
            break;

        case VT_CSI:
            if (c >= '0' && c <= '9') {
                if (vt->n_params < VT_MAX_PARAMS) {
                    vt->params[vt->n_params] =
                        vt->params[vt->n_params] * 10 + (uint32_t)(c - '0');
                    vt->has_param = true;
                }
            } else if (c == ';') {
                if (vt->has_param) vt->n_params++;
                else if (vt->n_params < VT_MAX_PARAMS) vt->n_params++;
                vt->has_param = false;
            } else if (c >= 0x40 && c <= 0x7E) {
                if (vt->has_param) vt->n_params++;
                dispatch_csi(vt, c);
                vt->pstate = VT_GROUND;
            }
            // Intermediate bytes (0x20..0x2F) ignored; stay in CSI.
            break;
    }

    spin_unlock(&vt->lock);
    if (ie) StiHelper();
}

void vt_switch_to(uint64_t idx) {
    if (idx >= NUM_VTS) return;
    vt_t* target = &vts[idx];
    if (target == active_vt) return;
    if (target->cells == NULL) return;

    bool ie = check_interrupts();
    CliHelper();
    // Take the target's lock for a coherent snapshot. The current active VT
    // won't be mutating its cells from another writer because anyone calling
    // vt_write_byte takes that VT's lock too — and we don't redraw the
    // outgoing VT, so we don't need to lock it.
    spin_lock(&target->lock);
    active_vt = target;
    if (target == &vts[0] && fb_owner != NULL) {
        // /dev/fb is mmap'd by a userspace renderer (term). Don't paint
        // vts[0]'s stale boot-log backbuffer over it — wake the owner with
        // SIGWINCH and let it repaint its own cell buffer. Drop the target
        // lock first; signal_send takes the run-queue lock.
        spin_unlock(&target->lock);
        signal_send(fb_owner, SIGWINCH);
    } else {
        fb_clear_all(target->cur_bg);
        fb_redraw_cells(target->cells, target->rows, target->cols);
        spin_unlock(&target->lock);
    }
    if (ie) StiHelper();
}

void vt_set_fb_owner(task_t* t) {
    fb_owner = t;
}

// ---- init -----------------------------------------------------------------

void vt_init_all(void) {
    uint64_t cols = 0, rows = 0;
    if (!fb_console_init(&cols, &rows)) return;

    uint64_t cells_bytes = rows * cols * sizeof(vt_cell_t);
    // Round up to a page so RequestBuddy is happy.
    uint64_t alloc_bytes = (cells_bytes + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    for (uint64_t i = 0; i < NUM_VTS; i++) {
        vt_t* vt = &vts[i];
        vt->lock.locked = 0;
        vt->cols   = cols;
        vt->rows   = rows;
        vt->cur_row = 0;
        vt->cur_col = 0;
        vt->cur_fg  = DEFAULT_FG;
        vt->cur_bg  = DEFAULT_BG;
        vt->cur_bold = false;
        vt->pstate  = VT_GROUND;
        vt->n_params = 0;
        vt->has_param = false;
        for (uint32_t k = 0; k < VT_MAX_PARAMS; k++) vt->params[k] = 0;

        // Allocate the backbuffer from the kernel buddy. The pool returns
        // physical addresses; convert to a kernel-virtual pointer so we can
        // write through the canonical kernel mapping.
        void* phys = RequestBuddy(alloc_bytes, false);
        if (phys == NULL) {
            kprintf("vt: failed to allocate backbuffer for VT%d\n", i);
            vt->cells = NULL;
            continue;
        }
        vt->cells = (vt_cell_t*)((uint64_t)phys + KERNEL_VIRTUAL);
        vt_cell_t blank = { .ch = ' ', .fg = vt->cur_fg, .bg = vt->cur_bg };
        for (uint64_t k = 0; k < rows * cols; k++) vt->cells[k] = blank;
    }

    active_vt = &vts[0];
    klog_vt   = &vts[0];
    fb_clear_all(active_vt->cur_bg);
}

void vt_klog_redirect(uint64_t idx) {
    if (idx >= NUM_VTS) return;
    if (vts[idx].cells == NULL) return;
    klog_vt = &vts[idx];
}
