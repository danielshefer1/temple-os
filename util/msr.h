#pragma once

#include "includes.h"

#define IA32_EFER             0xC0000080
#define IA32_STAR             0xC0000081
#define IA32_LSTAR            0xC0000082
#define IA32_FMASK            0xC0000084
#define IA32_GS_BASE          0xC0000101
#define IA32_KERNEL_GS_BASE   0xC0000102

#define EFER_SCE              (1ull << 0)

#define RFLAGS_IF             (1ull << 9)
#define RFLAGS_DF             (1ull << 10)

#define KERNEL_CS_SEL         0x08
#define KERNEL_DS_SEL         0x10
#define USER_DS_SEL           0x1B   // (slot 3 << 3) | RPL=3
#define USER_CS_SEL           0x23   // (slot 4 << 3) | RPL=3

extern uint64_t rdmsr(uint32_t msr);
extern void     wrmsr(uint32_t msr, uint64_t value);
extern void     LoadTSS(uint16_t selector);
extern void     syscall_entry(void);
