#pragma once
#include "includes.h"

#define NUM_VTS 6
#define VT_MAX_PARAMS 8

// Dedicated kernel-log VT. After start() finishes, kprintf/kerror are
// redirected here so their output stops clobbering the framebuffer once
// /bin/term takes it over. Switch the active VT to it to view the log.
#define KLOG_VT_IDX (NUM_VTS - 1)
