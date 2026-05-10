#pragma once
#include "includes.h"
#include "lock_types.h"
#include "vt_defs.h"
#include "vt_types.h"

extern vt_t  vts[NUM_VTS];
extern vt_t* active_vt;
extern vt_t* klog_vt;

struct task_t;
extern struct task_t* fb_owner;

// Record the userspace task that has /dev/fb mmap'd. vt_switch_to uses it
// to send SIGWINCH instead of redrawing vts[0]'s stale backbuffer over a
// live FB renderer. Pass NULL on /dev/fb close.
void vt_set_fb_owner(struct task_t* t);

// Initialise all VTs once fb_console primitives are ready (geometry + glyph
// size known). Allocates each VT's cell backbuffer via the kernel buddy.
void vt_init_all(void);

// Feed one byte to a VT's parser/renderer. If `vt == active_vt`, mutated
// cells are also blitted to the FB; otherwise the change only lives in the
// backbuffer and becomes visible on the next switch.
void vt_write_byte(vt_t* vt, char c);

// Switch the active VT. Triggers a full redraw of `vts[idx]`'s backbuffer
// onto the framebuffer. Out-of-range indices are silently ignored.
void vt_switch_to(uint64_t idx);

// Redirect the kernel-log target (kprintf/kerror) to vts[idx]. Picking a
// non-active index keeps kernel logs off the framebuffer once /bin/term
// takes the screen. Out-of-range / uninitialised indices are ignored.
void vt_klog_redirect(uint64_t idx);

// Shorthand for the common path: write a byte to the kernel-log VT. Safe
// to call before vt_init_all (no-op when klog_vt is NULL).
static inline void vt_write_klog_(char c) {
    extern vt_t* klog_vt;
    if (klog_vt) vt_write_byte(klog_vt, c);
}
