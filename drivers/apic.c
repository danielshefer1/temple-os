#include "apic.h"

static uint64_t ticks_per_ms;

void DisablePic() {
    // ICW1: start init, expect ICW4
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    // ICW2: vector offsets (master -> 0x20, slave -> 0x28)
    outb(MASTER_PIC, 0x20);
    outb(SLAVE_PIC,  0x28);
    // ICW3: cascade wiring (slave on master IRQ2)
    outb(MASTER_PIC, 0x04);
    outb(SLAVE_PIC,  0x02);
    // ICW4: 8086 mode
    outb(MASTER_PIC, 0x01);
    outb(SLAVE_PIC,  0x01);
    // Mask everything; EnablePitTimer will selectively unmask IRQ 0
    outb(MASTER_PIC, 0xFF);
    outb(SLAVE_PIC,  0xFF);
}
void EnablePitTimer() {
    outb(MASTER_PIC, 0xFE);
}

void InitPitTimer(uint64_t frequency) {
    EnablePitTimer();
    uint64_t divisor = 1193182 / frequency;


    outb(0x43, 0x36);

    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, low);
    outb(0x40, high);
}

uint64_t FindLapicTimerInitalCount() {
    InitPitTimer(PIC_TIMER_FREQUENCY); 

    pit_timer_fired = false;
    StiHelper();
    while (!pit_timer_fired) PauseHelper();

    uint64_t initial_count = 0xFFFFFFFF;
    lapic[0x380 / 4] = initial_count;

    pit_timer_fired = false;
    while (!pit_timer_fired) PauseHelper();

    uint64_t current_count = lapic[0x390 / 4];
    lapic[0x380 / 4] = 0; 

    DisablePic();
    CliHelper();

    uint64_t elapsed = initial_count - current_count;
    return elapsed / (1000 / PIC_TIMER_FREQUENCY);
}

void EnableLapic() {
    lapic[0xF0 / 4] = 0x1FF;
}

void InitTimer(uint64_t ms) {
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
    uint64_t cpu_id = get_cpuid();
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

void ioapic_write(uint8_t offset, uint64_t value) {
    ioapic[IOAPIC_REG_INDEX / 4] = offset;
    ioapic[IOAPIC_REG_DATA / 4] = value;
}

uint64_t get_gsi(uint8_t irq) {
    for (uint64_t i = 0; i < overrides_length; i++) {
        if (irq == overrides[i]->irq_source) return overrides[i]->global_system_interrupt;
    }
    return irq;
}

void IOAPIC_SetEntry(uint8_t irq, uint8_t vector) {
    uint64_t gsi = get_gsi(irq);
    uint64_t low_index = IOAPIC_REDTBL_BASE + (gsi * 2);
    uint64_t high_index = low_index + 1;

    // High 32 bits: Destination (usually 0 for the first CPU)
    ioapic_write(high_index, 0x00000000);

    // Low 32 bits: 
    // Bit 0-7: Vector
    // Bit 8-10: Delivery Mode (000 = Fixed)
    // Bit 11: Destination Mode (0 = Physical)
    // Bit 16: Mask (0 = Unmasked)
    uint64_t low_val = vector; // Logic: Fixed delivery, physical, unmasked
    ioapic_write(low_index, low_val);
}

void InitKeyboard() {
    IOAPIC_SetEntry(1, 33);
}