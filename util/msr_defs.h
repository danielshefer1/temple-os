#pragma once

#define IA32_EFER             0xC0000080
#define IA32_STAR             0xC0000081
#define IA32_LSTAR            0xC0000082
#define IA32_FMASK            0xC0000084
#define IA32_GS_BASE          0xC0000101
#define IA32_KERNEL_GS_BASE   0xC0000102
#define IA32_PAT              0x00000277

// PAT memory-type encodings (SDM Vol 3, Table 11-10).
#define PAT_TYPE_UC           0x00
#define PAT_TYPE_WC           0x01
#define PAT_TYPE_WT           0x04
#define PAT_TYPE_WP           0x05
#define PAT_TYPE_WB           0x06
#define PAT_TYPE_UC_MINUS     0x07

#define EFER_SCE              (1ull << 0)

#define RFLAGS_IF             (1ull << 9)
#define RFLAGS_DF             (1ull << 10)

#define KERNEL_CS_SEL         0x08
#define KERNEL_DS_SEL         0x10
#define USER_DS_SEL           0x1B   // (slot 3 << 3) | RPL=3
#define USER_CS_SEL           0x23   // (slot 4 << 3) | RPL=3
