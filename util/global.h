#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"

extern volatile bool pit_timer_fired;
extern uint64_t timer_ticks[UINT8_MAX];
extern volatile uint32_t* lapic;
extern volatile uint32_t* ioapic;
extern uint64_t cpu_count;
extern int_override_t* overrides[16];
extern volatile uint64_t overrides_length;
extern uint8_t cpu_ids[UINT8_MAX];
extern volatile uint64_t cpus_active;
extern volatile uint64_t ap_online_ack;
extern volatile pci_config_t* ecam_ptr;
extern volatile hba_mem_t* hba;
extern block_device_node_t* devices_head;
extern partition_device_node_t* parts_head;
extern volatile bool shutdown_req;