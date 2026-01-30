#include "global.h"

input_buffer_t console_buffer;
volatile bool shift_pressed = false;
volatile bool pit_timer_fired;
volatile uint32_t timer_ticks = 0;
volatile uint32_t* lapic;
volatile uint32_t* ioapic;
uint32_t cpu_count;

