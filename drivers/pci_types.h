#pragma once
#include "includes.h"

typedef struct pci_config_t{
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;
    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;

    uint32_t bars[6];

    uint32_t cardbus_cis_ptr;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base_addr;
    uint8_t  capabilities_ptr;
    uint8_t  reserved0[3];
    uint32_t reserved1;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_grant;
    uint8_t  max_latency;

    uint8_t  device_specific[4032];

} __attribute__((packed)) pci_config_t;

typedef struct {
    uint8_t  cap_id;
    uint8_t  next_ptr;      // Offset to next capability
    uint16_t message_ctl;   // Control bits
    uint32_t message_addr;  // Lower 32 bits of the address
    uint32_t message_addr_u; // (only if bit 7 of message_ctl is set)
    uint16_t message_data;  // vector number
    uint16_t reserved;
} __attribute__((packed)) msi_cap_t;
