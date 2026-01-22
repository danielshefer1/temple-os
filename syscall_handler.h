#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "keyboard.h"
#include "global.h"
#include "buddy_alloc.h"

void syscall_handler(interrupt_frame_t* frame);

// SysCall Handlers
int32_t WriteHandler(interrupt_frame_t* frame);
int32_t ReadHandler(interrupt_frame_t* frame);
int32_t MmapHandler(interrupt_frame_t* frame);
int32_t MunmapHandler(interrupt_frame_t* frame);
int32_t UnknownSysCall();
int32_t ExitHandler();
int32_t FlushBufferHandler();