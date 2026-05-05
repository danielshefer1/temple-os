#pragma once
#include "includes.h"

typedef struct rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;

    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];

} __attribute__((packed)) rsdp_t;

typedef struct acpi_header_t {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

typedef struct rsdt_t {
    acpi_header_t header;
    uint32_t entries[];
} __attribute__((packed)) rsdt_t;

typedef struct madt_t {
    acpi_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
} __attribute__((packed)) madt_t;

typedef struct madt_entry_header_t {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

typedef struct fadt_t {
    acpi_header_t header;

    uint32_t firmware_ctrl;      // Physical address of FACS
    uint32_t dsdt;               // Physical address of DSDT
    uint8_t  reserved1;          // Used in ACPI 1.0; reserved now
    uint8_t  preferred_pm_profile;
    uint16_t sci_int;            // System Control Interrupt (IRQ)
    uint32_t smi_cmd;            // Port to write to for SMI commands
    uint8_t  acpi_enable;        // Value to write to smi_cmd to enable ACPI
    uint8_t  acpi_disable;       // Value to write to smi_cmd to disable ACPI
    uint8_t  s4bios_req;         // Value for S4BIOS state
    uint8_t  pstate_cnt;         // Processor performance state control
    uint32_t pm1a_evt_blk;       // PM1a Event Block Address
    uint32_t pm1b_evt_blk;       // PM1b Event Block Address
    uint32_t pm1a_cnt_blk;       // PM1a Control Block Address
    uint32_t pm1b_cnt_blk;       // PM1b Control Block Address
    uint32_t pm2_cnt_blk;        // PM2 Control Block Address
    uint32_t pm_tmr_blk;         // PM Timer Block Address
    uint32_t gpe0_blk;           // General Purpose Event 0 Block
    uint32_t gpe1_blk;           // General Purpose Event 1 Block
    uint8_t  pm1_evt_len;
    uint8_t  pm1_cnt_len;
    uint8_t  pm2_cnt_len;
    uint8_t  pm_tmr_len;
    uint8_t  gpe0_blk_len;
    uint8_t  gpe1_blk_len;
    uint8_t  gpe1_base;
    uint8_t  cst_cnt;            // _CST support
    uint16_t p_lvl2_lat;         // Worst-case latency to enter C2
    uint16_t p_lvl3_lat;         // Worst-case latency to enter C3
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t  duty_offset;
    uint8_t  duty_width;
    uint8_t  day_alrm;           // RTC Day Alarm index
    uint8_t  mon_alrm;           // RTC Month Alarm index
    uint8_t  century;            // RTC Century index (The one you need!)
    uint16_t boot_arch_flags;    // Legacy Boot Arch Flags (PS/2, VGA, etc)
    uint8_t  reserved2;
    uint32_t flags;              // Fixed Feature Flags (e.g. HW_REDUCED_ACPI)
} __attribute__((packed)) fadt_t;

typedef struct local_apic_t {
    madt_entry_header_t header;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint64_t flags;
} __attribute__((packed)) local_apic_t;

typedef struct io_apic_t {
    madt_entry_header_t header;
    uint8_t ioapic_id;
    uint8_t reserved;
    uint64_t ioapic_address;
    uint64_t global_system_interrupt_base;
} __attribute__((packed)) io_apic_t;

typedef struct int_override_t {
    madt_entry_header_t header;
    uint8_t bus_source;
    uint8_t irq_source;
    uint64_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed)) int_override_t;

typedef struct madt_local_apic_nmi_t {
    madt_entry_header_t header;
    uint8_t acpi_processor_id;
    uint16_t flags;
    uint8_t lint;
} __attribute__((packed)) madt_local_apic_nmi_t;

typedef struct mcfg_entry_t {
    uint64_t base_address;
    uint16_t pci_segment_group;
    uint8_t start_bus_number;
    uint8_t end_bus_number;
    uint32_t reserved;
} __attribute__((packed)) mcfg_entry_t;

typedef struct mcfg_t {
    acpi_header_t header;
    uint64_t reserved;
    mcfg_entry_t entries[];
} __attribute__((packed)) mcfg_t;
