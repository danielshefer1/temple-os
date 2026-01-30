#include "timer.h"

void sleep(uint32_t ms) {
    uint8_t cpu_id = get_cpuid();
    uint32_t start = timer_ticks[cpu_id];
    
    // Unsigned subtraction handles the wrap-around automatically!
    while ((timer_ticks[cpu_id] - start) * TIMER_TICK_PER_MS < ms) {
        PauseHelper();
    }
}

void WaitSeconds(uint32_t seconds) {
    sleep(seconds * SEC);
}