#include "global.h"

buffer_queue console_buffer;
volatile bool shift_pressed = false;
volatile uint32_t timer_ticks = 0;