#include "apic.h"

static uint32_t ticks_per_ms;

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

    if (ticks_per_ms == 0) ticks_per_ms = FindLapicTimerInitalCount();

    // Replace the timer func in IDT
    uint32_t cpu_id = get_cpuid();
    if (cpu_id == 0) ReplaceTimer();
    // Send dummy EOI
    lapic[0x0B0 / 4] = 0;
    // Set Timer divider to 16
    lapic[0x3E0 / 4] = 0x03;
    // Set IDT index and mode
    lapic[0x320 / 4] = 0x20 | (1 << 17);
    // Set Initial Count
    lapic[0x380 / 4] = ticks_per_ms * ms;
}

void ioapic_write(uint8_t offset, uint32_t value) {
    ioapic[IOAPIC_REG_INDEX / 4] = offset;
    ioapic[IOAPIC_REG_DATA / 4] = value;
}

uint32_t get_gsi(uint8_t irq) {
    for (uint32_t i = 0; i < overrides_length; i++) {
        if (irq == overrides[i]->irq_source) return overrides[i]->global_system_interrupt;
    }
    return irq;
}

void IOAPIC_SetEntry(uint8_t irq, uint8_t vector) {
    uint32_t gsi = get_gsi(irq);
    uint32_t low_index = IOAPIC_REDTBL_BASE + (gsi * 2);
    uint32_t high_index = low_index + 1;

    // High 32 bits: Destination (usually 0 for the first CPU)
    ioapic_write(high_index, 0x00000000);

    // Low 32 bits: 
    // Bit 0-7: Vector
    // Bit 8-10: Delivery Mode (000 = Fixed)
    // Bit 11: Destination Mode (0 = Physical)
    // Bit 16: Mask (0 = Unmasked)
    uint32_t low_val = vector; // Logic: Fixed delivery, physical, unmasked
    ioapic_write(low_index, low_val);
}

void InitKeyboard() {
    IOAPIC_SetEntry(1, 33);
}