#include "global.h"

volatile bool pit_timer_fired;
uint64_t timer_ticks[UINT8_MAX];
volatile uint32_t* lapic;
volatile uint32_t* ioapic;
uint64_t cpu_count;

int_override_t* overrides[16];
volatile uint64_t overrides_length;

uint8_t cpu_ids[UINT8_MAX];
volatile uint64_t cpus_active = 1;
volatile uint64_t ap_online_ack = 0;

volatile pci_config_t* ecam_ptr;

volatile hba_mem_t* hba;

block_device_node_t* devices_head;
partition_device_node_t* parts_head;

volatile bool shutdown_req = false;

fb_info_t fb_info = {0};