#pragma once

#include "includes.h"
#include "msr_defs.h"

extern uint64_t rdmsr(uint32_t msr);
extern void     wrmsr(uint32_t msr, uint64_t value);
extern void     LoadTSS(uint16_t selector);
extern void     syscall_entry(void);
