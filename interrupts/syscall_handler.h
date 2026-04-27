#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "keyboard.h"
#include "global.h"
#include "buddy_alloc.h"
#include "vfs_syscalls.h"

void syscall_handler(interrupt_frame_t* frame);

// SysCall Handlers
int64_t MmapHandler(interrupt_frame_t* frame);
int64_t MunmapHandler(interrupt_frame_t* frame);
int64_t UnknownSysCall();
int64_t ExitHandler();
int64_t FlushBufferHandler();