#include "timer.h"

void sleep(uint32_t ms) {
    uint32_t start = timer_ticks;
    
    // Unsigned subtraction handles the wrap-around automatically!
    while ((timer_ticks - start) < ms) {
        HltHelper();
    }
}

void WaitSeconds(uint32_t seconds) {
    sleep(seconds * ONE_SEC);
}