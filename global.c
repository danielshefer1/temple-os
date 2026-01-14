#include "global.h"

volatile InputBuffer console_buffer;
volatile bool shift_pressed = false;
volatile uint32_t timer_ticks = 0;