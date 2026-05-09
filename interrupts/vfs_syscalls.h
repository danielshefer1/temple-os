#pragma once

#include "types.h"
#include "defintions.h"

#include "fd_table.h"
#include "vfs.h"
#include "vga.h"
#include "keyboard.h"
#include "global.h"

int64_t SysOpen     (interrupt_frame_t* f);
int64_t SysClose    (interrupt_frame_t* f);
int64_t SysFRead    (interrupt_frame_t* f);
int64_t SysFWrite   (interrupt_frame_t* f);
int64_t SysLseek    (interrupt_frame_t* f);
int64_t SysTruncate (interrupt_frame_t* f);
int64_t SysUnlink   (interrupt_frame_t* f);
int64_t SysMkdir    (interrupt_frame_t* f);
int64_t SysRmdir    (interrupt_frame_t* f);
int64_t SysRename   (interrupt_frame_t* f);
int64_t SysSymlink  (interrupt_frame_t* f);
int64_t SysReadlink (interrupt_frame_t* f);
int64_t SysStat     (interrupt_frame_t* f);
int64_t SysSync     (interrupt_frame_t* f);
int64_t SysIoctl    (interrupt_frame_t* f);
int64_t SysMknod    (interrupt_frame_t* f);
int64_t SysChdir    (interrupt_frame_t* f);
int64_t SysGetcwd   (interrupt_frame_t* f);
int64_t SysGetdents (interrupt_frame_t* f);
