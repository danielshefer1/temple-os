#include "apic.h"

void DisablePic() {
    outb(MASTER_PIC, 0xFF);
    outb(SLAVE_PIC, 0xFF);
}
void EnablePitTimer() {
    outb(MASTER_PIC, 0xFE);
}

void InitPitTimer(uint32_t frequency) {
    EnablePitTimer();
    uint32_t divisor = 1193182 / frequency;


    outb(0x43, 0x36);

    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, low);
    outb(0x40, high);
}

uint32_t FindLapicTimerInitalCount() {
    InitPitTimer(PIC_TIMER_FREQUENCY); 

    pit_timer_fired = false;
    while (!pit_timer_fired) PauseHelper();

    uint32_t initial_count = 0xFFFFFFFF;
    lapic[0x380 / 4] = initial_count;

    pit_timer_fired = false;
    while (!pit_timer_fired) PauseHelper();

    uint32_t current_count = lapic[0x390 / 4];
    lapic[0x380 / 4] = 0; 

    DisablePic();

    uint32_t elapsed = initial_count - current_count;
    return elapsed / (1000 / PIC_TIMER_FREQUENCY);
}

void EnableLapic() {
    lapic[0xF0 / 4] = 0x1FF;
}

void InitTimer(uint32_t ms) {
    if (lapic == NULL) {
        kprintf("Find the lapic first by calling InitMadt()!\n");
        return;
    }
    // Set Timer divider to 16
    lapic[0x3E0 / 4] = 0x03;
    // Mask Timer
    lapic[0x320 / 4] = (1 << 16);    

    uint32_t ticks_per_ms = FindLapicTimerInitalCount();

    kprintf("Timer ticks per milisecond: %d", ticks_per_ms);
    // Replace the timer func in IDT
    ReplaceTimer();

    // Send dummy EOI
    lapic[0x0B0 / 4] = 0;
    // Set Timer divider to 16
    lapic[0x3E0 / 4] = 0x03;
    // Set IDT index and mode
    lapic[0x320 / 4] = 0x20 | (1 << 17);
    // Set Initial Count
    lapic[0x380 / 4] = ticks_per_ms * ms;
}