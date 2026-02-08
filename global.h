#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"

extern input_buffer_t console_buffer;
extern timed_key_t keyboard_buffer[CONSOLE_BUFFER_SIZE];
extern volatile bool shift_pressed;
extern volatile bool pit_timer_fired;
extern uint64_t timer_ticks[UINT8_MAX];
extern volatile uint64_t* lapic;
extern volatile uint64_t* ioapic;
extern uint64_t cpu_count;
extern int_override_t* overrides[16];
extern volatile uint64_t overrides_length;
extern uint8_t cpu_ids[UINT8_MAX];
extern volatile uint64_t cpus_active;
extern volatile pci_config_t* ecam_ptr;