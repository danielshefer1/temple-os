#include "ap_start.h"
#include "cpu_local.h"

void CopyTrampoline() {
    void* trampoline_binary_addr = (void*)&trampoline_binary;
    memcpy((void*)(TRAMPOLINE_ADDR + KERNEL_VIRTUAL), trampoline_binary_addr, trampoline_size);
}

void lapic_write_icr(uint32_t high, uint32_t low) {
    // High must be written first!
    lapic[0x310 / 4] = high;
    // Writing to the low register actually triggers the interrupt
    lapic[0x300 / 4] = low;

    // Wait for the 'Delivery Status' bit (bit 12) to clear
    // This ensures the IPI was actually sent
    while (lapic[0x300 / 4] & (1 << 12)) {
        PauseHelper();
    }
}

void SendInitIPI(uint32_t apic_id) {
    // 0x0000C500 breaks down as:
    // Bits 8-10: 101 (INIT)
    // Bit 14: 1 (Assert)
    // Bit 15: 0 (Level trigger)
    lapic_write_icr(apic_id << 24, 0x0000C500);
}

void SendStartupIPI(uint32_t apic_id, uint8_t vector) {
    // 0x00000600 | vector
    // Bits 8-10: 110 (Startup)
    lapic_write_icr(apic_id << 24, 0x00000600 | vector);
}

void BootCore(uint8_t cpu_id, bool trampoline_set) {
    bool found_cpu = false;
    uint64_t idx = 0;

    for (uint64_t i = 1; i < cpu_count; i++) {
        if (cpu_ids[i] == cpu_id) {
            found_cpu = true;
            idx = i;
            break;
        }
    }
    if (!found_cpu) {
        kprintf("Cpu ID doesn't exist\n");
        return;
    }
    if (!trampoline_set) CopyTrampoline();

    uint64_t* inputs = (uint64_t*)(TRAMPOLINE_ADDR + KERNEL_VIRTUAL + trampoline_size);
    uint64_t stack_base = AddStack();
    cpu_locals[idx].kstack_top = stack_base + STACK_PAGES * PAGE_SIZE;
    inputs[0] = stack_base; // this cpu's stack (consumed by trampoline)
    inputs[1] = (uint64_t)((void*)ap_kmain); 
    inputs[2] = ((uint64_t) PageDirAddrV()) - KERNEL_VIRTUAL;

    //CliHelper();

    SendInitIPI(cpu_id);
    
    sleep(10); 

    SendStartupIPI(cpu_id, TRAMPOLINE_ADDR / PAGE_SIZE);
    

    for(volatile uint64_t i = 0; i < 10000; i++) {
        PauseHelper();
    }

    //SendStartupIPI(cpu_id, TRAMPOLINE_ADDR / PAGE_SIZE);

    //StiHelper();
}

void BootCores() {
    if (cpu_count == 1) return;
    BootCore(cpu_ids[1], false);
    while (cpus_active == 1) PauseHelper();
    for (uint64_t i = 2; i < cpu_count; i++) {
        uint64_t current_cpus_active = cpus_active;
        BootCore(cpu_ids[i], true);
        while (current_cpus_active == cpus_active) PauseHelper();
    }
}