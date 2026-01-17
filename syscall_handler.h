#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "keyboard.h"
#include "global.h"
#include "buddy_alloc.h"

void syscall_handler(interrupt_frame* frame);

// SysCall Handlers
uint32_t WriteHandler(interrupt_frame* frame);
uint32_t ReadHandler(interrupt_frame* frame);
uint32_t MmapHandler(interrupt_frame* frame);
uint32_t MunmapHandler(interrupt_frame* frame);
uint32_t UnknownSysCall();
uint32_t HltSYSCALLHandler();
uint32_t FlushBufferHandler();