#include "global.h"

input_buffer_t console_buffer;
volatile bool shift_pressed = false;
volatile bool pit_timer_fired;
uint32_t timer_ticks[UINT8_MAX];
volatile uint32_t* lapic;
volatile uint32_t* ioapic;
uint32_t cpu_count;

int_override_t* overrides[16];
volatile uint32_t overrides_length;

uint8_t cpu_ids[UINT8_MAX];
volatile uint32_t cpus_active = 1;

volatile pci_config_t* ecam_ptr;