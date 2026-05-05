#pragma once
#include "includes.h"
#include "lock_types.h"

#define AHCI_MAX_SLOTS 32

typedef struct {
    uint32_t clb;       // Command List Base Address (Low)
    uint32_t clbu;      // Command List Base Address (Upper)
    uint32_t fb;        // FIS Base Address (Low)
    uint32_t fbu;       // FIS Base Address (Upper)
    uint32_t is;        // Interrupt Status
    uint32_t ie;        // Interrupt Enable
    uint32_t cmd;       // Command and Status
    uint32_t reserved0;
    uint32_t tfd;       // Task File Data
    uint32_t sig;       // Signature
    uint32_t ssts;      // SATA Status (SCR0: SStatus)
    uint32_t sctl;      // SATA Control (SCR2: SControl)
    uint32_t serr;      // SATA Error (SCR1: SError)
    uint32_t sact;      // SATA Active (SCR3: SActive)
    uint32_t ci;        // Command Issue
    uint32_t sntf;      // SATA Notification (SCR4: SNotification)
    uint32_t fbs;       // FIS-based Switching Control
    uint32_t reserved1[11];
    uint32_t vendor[4]; // Vendor specific
} __attribute__((packed)) hba_port_t;

typedef struct {
    uint32_t cap;       // Host Capabilities
    uint32_t ghc;       // Global Host Control
    uint32_t is;        // Interrupt Status
    uint32_t pi;        // Ports Implemented
    uint32_t vs;        // Version
    uint32_t ccc_ctl;   // Command Completion Coalescing Control
    uint32_t ccc_pts;   // Command Completion Coalescing Ports
    uint32_t em_loc;    // Enclosure Management Location
    uint32_t em_ctl;    // Enclosure Management Control
    uint32_t cap2;      // Host Capabilities Extended
    uint32_t bohc;      // BIOS/OS Handoff Control and Status

    uint8_t  reserved[0xA0 - 0x2C]; // Padding

    uint8_t  vendor_specific[0x100 - 0xA0];

    hba_port_t ports[32];
} __attribute__((packed)) hba_mem_t;

typedef struct {
    uint32_t dba;       // Data Base Address (Low 32 bits)
    uint32_t dbau;      // Data Base Address Upper (High 32 bits)
    uint32_t reserved0;
    // Bits 0-21: Byte count (0-based, so 4095 = 4KB)
    // Bit 31: Interrupt on Completion
    uint32_t dw3;
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct {
    uint8_t  fis_type;   // 0x27 (Register H2D)
    uint8_t  pmport:4;   // Port multiplier
    uint8_t  reserved0:3;
    uint8_t  c:1;        // 1 = Command, 0 = Control
    uint8_t  command;    // Actual command (e.g., 0x25 for READ DMA EXT)
    uint8_t  featurel;   // Feature low

    uint8_t  lba0;       // LBA 0-7
    uint8_t  lba1;       // LBA 8-15
    uint8_t  lba2;       // LBA 16-23
    uint8_t  device;     // Device register

    uint8_t  lba3;       // LBA 24-31
    uint8_t  lba4;       // LBA 32-39
    uint8_t  lba5;       // LBA 40-47
    uint8_t  featureh;   // Feature high

    uint8_t  countl;     // Count low
    uint8_t  counth;     // Count high
    uint8_t  icc;        // Isochronous command completion
    uint8_t  control;    // Control register

    uint8_t  reserved1[4];
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct {
    // 64 bytes of FIS area
    uint8_t  cfis[64];    // Command FIS (usually a fis_reg_h2d_t)

    // 16 bytes of ATAPI command (only for CD/DVD drives)
    uint8_t  acmd[16];

    uint8_t  reserved[48];

    // Physical Region Descriptor Table (PRDT)
    hba_prdt_entry_t prdt_entries[];
} __attribute__((packed)) hba_cmd_table_t;

typedef struct {
    // DW0
    uint8_t  cfl:5;      // Command FIS Length (in DWORDS, 5 for H2D)
    uint8_t  a:1;        // ATAPI
    uint8_t  w:1;        // Write (1 = Host to Device)
    uint8_t  p:1;        // Prefetchable
    uint8_t  r:1;        // Reset
    uint8_t  b:1;        // BIST
    uint8_t  c:1;        // Clear Busy upon R_OK
    uint8_t  reserved0:1;
    uint8_t  pmp:4;      // Port Multiplier Port
    uint16_t prdtl;      // PRDT Length (Number of entries in the table)

    // DW1
    uint32_t prdbc;      // Physical Region Descriptor Byte Count (Transferred so far)

    // DW2 & 3
    uint32_t ctba;       // Command Table Base Address (Low)
    uint32_t ctbau;      // Command Table Base Address Upper (High)

    // DW4 - 7
    uint32_t reserved1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct ahci_completion_t {
    spinlock_t guard;
    volatile bool done;
    volatile bool error;
    struct task_t* waiter;
} ahci_completion_t;

typedef struct ahci_port_state_t {
    spinlock_t lock;          // serializes slot allocation + CI write + issued_mask
    uint32_t issued_mask;     // slots currently in flight (we own them)
    ahci_completion_t completions[AHCI_MAX_SLOTS];
} ahci_port_state_t;
