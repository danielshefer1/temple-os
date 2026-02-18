#include "global.h"

input_buffer_t console_buffer;
timed_key_t keyboard_buffer[CONSOLE_BUFFER_SIZE];

volatile bool shift_pressed = false;
volatile bool pit_timer_fired;
uint64_t timer_ticks[UINT8_MAX];
volatile uint32_t* lapic;
volatile uint32_t* ioapic;
uint64_t cpu_count;

int_override_t* overrides[16];
volatile uint64_t overrides_length;

uint8_t cpu_ids[UINT8_MAX];
volatile uint64_t cpus_active = 1;

volatile pci_config_t* ecam_ptr;

volatile hba_mem_t* hba;