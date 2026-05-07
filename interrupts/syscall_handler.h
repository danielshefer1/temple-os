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
int64_t ExitHandler(interrupt_frame_t* frame);
int64_t WaitpidHandler(interrupt_frame_t* frame);
int64_t FlushBufferHandler();
int64_t ExecHandler(interrupt_frame_t* frame);
int64_t SpawnHandler(interrupt_frame_t* frame);
int64_t ForkHandler(interrupt_frame_t* frame);
int64_t KillHandler(interrupt_frame_t* frame);
int64_t SignalHandler(interrupt_frame_t* frame);
int64_t SigreturnHandler(interrupt_frame_t* frame);
int64_t GetpidHandler(interrupt_frame_t* frame);