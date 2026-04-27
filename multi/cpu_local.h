#pragma once

#include "includes.h"
#include "types.h"

#define MAX_CPUS 64

extern cpu_local_t cpu_locals[MAX_CPUS];
extern uint8_t apic_to_index[256];

void cpu_init_late(uint32_t idx);
cpu_local_t* this_cpu(void);
