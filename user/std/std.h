#pragma once

// stdtemple umbrella header. Pulls in everything a typical user program
// needs: a SysV-aware _start, syscall wrappers, and string/print helpers.
//
// Programs that ship their own _start (init, term, hello) define
// ST_NO_START before including this header.

#ifndef ST_NO_START
#include "std/start.h"
#endif

#include "std/syscalls.h"
#include "std/string.h"
#include "std/stdio.h"
#include "std/errno.h"
#include "std/signal.h"
#include "std/fcntl.h"
#include "std/sys/types.h"
#include "std/sys/wait.h"
#include "std/sys/dirent.h"
#include "std/sys/stat.h"
#include "std/sys/ioctl.h"
