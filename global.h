#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"

extern input_buffer_t console_buffer;
extern volatile bool shift_pressed;
extern volatile bool pit_timer_fired;
extern uint32_t timer_ticks[UINT8_MAX];
extern volatile uint32_t* lapic;
extern volatile uint32_t* ioapic;
extern uint32_t cpu_count;
extern int_override_t* overrides[16];
extern volatile uint32_t overrides_length;
extern uint8_t cpu_ids[UINT8_MAX];
extern volatile uint32_t cpus_active;